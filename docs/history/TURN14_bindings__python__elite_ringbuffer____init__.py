"""ELITEIPC 1.0.1: ctypes control/data API with tracked zero-copy memoryviews.

Memoryviews retain both slot ownership and the mapped endpoint. Release every
view (including derived views) before commit/release. Context exits abort an
uncommitted write and release a read; they never publish an unfinished write.
Endpoint calls are serialized by a local RLock. Concurrent access through raw
buffer aliases remains the application's responsibility. Fork inheritance is
not an attachment; use spawn/exec and trusted constructed grants.
"""
from __future__ import annotations

import atexit
import base64
import ctypes as C
import os
import threading
import time
import uuid
import warnings
import weakref
from typing import Any, Mapping

from . import _native as N
try:
    import _elite_buffer
except ImportError as exc:
    raise ImportError("Build the tracked exporter with make python and add BUILD/python to PYTHONPATH") from exc

if getattr(_elite_buffer, "API_VERSION", None) != 0x00010001:
    raise ImportError("ELITE requires the matching 1.0.1 tracked exporter; rebuild make python")

__version__ = "1.0.1"
__all__ = ["EliteShm", "EliteSpscProducer", "EliteSpscConsumer", "EliteNcqProducer",
           "EliteNcqConsumer", "WriteLease", "ReadLease", "EliteError", "BusyError",
           "WouldBlock", "RetiredError", "cleanup_failures", "__version__"]
# Fail-closed references for genuinely failed cleanup. Not a recovery daemon.
# A retained entry is inspectable; no forced token return or guessed unmapping.
_FAILED_CLEANUP: list[tuple[Any, str]] = []
_LIVE_SHMS: weakref.WeakSet = weakref.WeakSet()


class EliteError(RuntimeError):
    """Native failure with an independent ownership outcome and OS error."""
    def __init__(self, result: N.Result, operation: str, resource: Any = None):
        self.status = int(result.status)
        self.outcome = int(result.outcome)
        self.os_error = int(result.os_error)
        self.resource = resource
        name = N.lib.elite_status_string(self.status).decode("ascii")
        super().__init__(f"{operation}: {name}; outcome={self.outcome}; os_error={self.os_error}")


class BusyError(EliteError):
    """A lease, view, pending grant, or other cleanup obligation is still live."""


class WouldBlock(EliteError):
    """No data or free capacity was observed within the caller's retry budget."""


class RetiredError(EliteError):
    """This generation no longer accepts the requested new work."""


def _check(r: N.Result, operation: str, resource: Any = None) -> N.Result:
    if r.status:
        cls = BusyError if r.status == 4 else WouldBlock if r.status in (1, 2) else RetiredError if r.status == 3 else EliteError
        raise cls(r, operation, resource)
    return r


def _uint(value: int, bits: int, name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value < 1 << bits:
        raise ValueError(f"{name} must be an unsigned {bits}-bit integer")
    return value


def cleanup_failures() -> tuple[str, ...]:
    """Unresolved local cleanup diagnostics. No token or holder is auto-reclaimed."""
    return tuple(text for _, text in _FAILED_CLEANUP)


def _retain_failure(resource: Any, exc: BaseException) -> None:
    _FAILED_CLEANUP.append((resource, repr(exc)))
    warnings.warn(f"ELITE retained a cleanup obligation: {exc}", ResourceWarning, stacklevel=2)


def _encode(obj: C.Structure) -> dict[str, Any]:
    out = {}
    for name, typ in obj._fields_:
        value = getattr(obj, name)
        if typ is C.c_char * 31:
            out[name] = bytes(value).decode("ascii")
        elif isinstance(value, C.Array):
            out[name] = bytes(value).hex()
        else:
            out[name] = int(value)
    return out


def _decode(typ: type[C.Structure], data: Mapping[str, Any]) -> C.Structure:
    """Fieldwise control encoding, NOT native-struct serialization/authentication."""
    if not isinstance(data, Mapping) or set(data) != {n for n, _ in typ._fields_}:
        raise ValueError(f"incorrect {typ.__name__} control fields")
    obj = typ()
    for name, field_type in typ._fields_:
        value = data[name]
        if issubclass(field_type, C.Array):
            if field_type._type_ is C.c_char:
                if not isinstance(value, str): raise ValueError("invalid shm name")
                raw = value.encode("ascii")
                if len(raw) != 30 or b"\0" in raw: raise ValueError("invalid shm name")
                setattr(obj, name, raw)
            else:
                if not isinstance(value, str) or len(value) != 32: raise ValueError("invalid identity")
                raw = bytes.fromhex(value)
                setattr(obj, name, field_type.from_buffer_copy(raw))
        else:
            setattr(obj, name, _uint(value, C.sizeof(field_type) * 8, name))
    return obj


class _Authority:
    def __init__(self, max_backing_bytes: int):
        self.pid = os.getpid()
        self.lock = threading.RLock()
        self.ptr = N.P()
        self.objects: weakref.WeakSet[EliteShm] = weakref.WeakSet()
        self.max_backing_bytes = max_backing_bytes
        host = N.ID.from_buffer_copy(uuid.uuid4().bytes)
        authority = N.ID.from_buffer_copy(uuid.uuid4().bytes)
        _check(N.lib.elite_authority_create(host, authority, max_backing_bytes, C.byref(self.ptr)), "authority_create")

    def check(self) -> None:
        if self.pid != os.getpid(): raise RuntimeError("inherited authority: use a registered spawned child")
        if not self.ptr: raise RuntimeError("authority is closed")

    def __del__(self):
        try:
            if self.pid == os.getpid() and self.ptr:
                _check(N.lib.elite_authority_destroy(C.byref(self.ptr)), "authority_destroy")
        except BaseException as exc:
            _retain_failure(self, exc)


class _ObjectLifetime:
    """Native object ownership, independent of a public manager's user graph."""
    def __init__(self, ptr, authority):
        self._ptr, self.authority = ptr, authority
        self.pid = os.getpid()
        self.connections = weakref.WeakSet()
        self.close_requested = False

    def ack(self, receipt):
        with self.authority.lock:
            self.authority.check()
            if self._ptr:
                _check(N.lib.elite_object_ack_cleanup(self._ptr, C.byref(receipt)), "ack_cleanup", self)

    def close(self):
        if not self._ptr: return
        if any(c._ptr for c in list(self.connections)):
            raise BusyError(N.Result(4, 5, 0, 0), "object lifetime: active endpoint", self)
        with self.authority.lock:
            self.authority.check()
            _check(N.lib.elite_object_destroy(C.byref(self._ptr)), "object_destroy", self)

    def request_close(self):
        self.close_requested = True
        self.try_close()

    def try_close(self):
        if self.pid != os.getpid() or not self.close_requested or not self._ptr: return
        if any(c._ptr for c in list(self.connections)): return
        try: self.close()
        except BaseException as exc: _retain_failure(self, exc)


class _EndpointLifetime:
    """No strong edges to the public endpoint, manager, lease, or exporter.

    Retained by wrapper finalizers and lease state. Therefore GC cannot clear
    the native cleanup data while it is clearing an application reference cycle.
    """
    def __init__(self, ptr, lock, pid, owner):
        self._ptr, self._lock, self._pid = ptr, lock, pid
        self.owner = owner
        self._pin = None
        self.receipt = None
        self.close_requested = False
        self.poisoned = False
        if owner is not None: owner.connections.add(self)

    def _check(self):
        if self._pid != os.getpid(): raise RuntimeError("inherited endpoint: spawn/exec required")
        if not self._ptr: raise RuntimeError("endpoint is closed")
        if self.poisoned: raise EliteError(N.Result(12, 6, 0, 0), "endpoint outcome is uncertain", self)

    @property
    def busy(self):
        pin = self._pin() if self._pin else None
        return self.poisoned or (pin is not None and (pin.active or pin.acquiring))

    def close(self):
        with self._lock:
            if not self._ptr: return self.receipt
            self._check()
            if self.busy: raise BusyError(N.Result(4, 5, 0, 0), "endpoint.close: outstanding lease/view", self)
            receipt = N.Receipt()
            _check(N.lib.elite_detach(C.byref(self._ptr), C.byref(receipt)), "detach", self)
            self.receipt = _encode(receipt)
            if self.owner is not None:
                self.owner.ack(receipt)
                self.owner.try_close()
            return self.receipt

    def request_close(self):
        self.close_requested = True
        self.try_close()

    def try_close(self):
        if self._pid != os.getpid() or not self.close_requested or not self._ptr or self.busy: return
        try: self.close()
        except BaseException as exc: _retain_failure(self, exc)


class EliteShm:
    """Application-owned managed generation. Not a name-only attach primitive.

    Initial construction creates an authority. successor() shares that authority's
    four-object/two-quarantine budget. grant() must target self or an owned child
    registered before exposure. Peer crashes are fenced by reap(), never timeouts.
    """
    def __init__(self, mode: str = "spsc", capacity: int = 1024,
                 max_payload: int = 64, producers: int = 1, consumers: int = 1,
                 checksum: bool = False, parkable: bool = False,
                 max_backing_bytes: int = 268435456, *, _authority: _Authority | None = None):
        if mode not in ("spsc", "ncq"): raise ValueError("mode must be spsc or ncq")
        for name, value, bits in (("capacity", capacity, 64), ("max_payload", max_payload, 32),
                                  ("producers", producers, 32), ("consumers", consumers, 32),
                                  ("max_backing_bytes", max_backing_bytes, 64)):
            _uint(value, bits, name)
        if not 1 <= producers <= capacity or not 1 <= consumers <= capacity or producers + consumers > capacity:
            raise ValueError("positive endpoint counts must sum to at most capacity")
        if mode == "spsc" and (producers != 1 or consumers != 1): raise ValueError("SPSC requires 1P/1C")
        if parkable and mode != "spsc": raise ValueError("MPMC parking is not an admitted protocol")
        self._pid = os.getpid(); self._ptr = N.P()
        self._authority = _authority or _Authority(max_backing_bytes)
        self._life = _ObjectLifetime(self._ptr, self._authority)
        self._finalizer = weakref.finalize(self, self._life.request_close)
        self._local: weakref.WeakSet[_Endpoint] = weakref.WeakSet()
        self._issued: dict[int, int] = {}
        self.mode, self.capacity, self.max_payload = mode, capacity, max_payload
        self.producers, self.consumers = producers, consumers
        self.checksum, self.parkable = bool(checksum), bool(parkable)
        cfg = N.Config(1 if mode == "spsc" else 2, int(parkable), int(checksum),
                       max_payload, capacity, producers, consumers, time.time_ns())
        defs = (N.Definition * (producers + consumers))()
        for i, d in enumerate(defs):
            d.endpoint_id = N.ID.from_buffer_copy(uuid.uuid4().bytes)
            d.process_incarnation_id = N.ID.from_buffer_copy(uuid.uuid4().bytes)
            d.role = 1 if i < producers else 2
        with self._authority.lock:
            self._authority.check()
            _check(N.lib.elite_create(self._authority.ptr, C.byref(cfg), defs, C.byref(self._ptr)), "create", self)
            _check(N.lib.elite_object_activate(self._ptr), "activate", self)
            n, b, p, session = N.U64(), N.U32(), N.U32(), N.ID()
            _check(N.lib.elite_binding_info(None, self._ptr, C.byref(n), C.byref(b), C.byref(p), session), "object_info", self)
            self.session_id = bytes(session).hex()
            self.name = "/el-" + base64.b32encode(bytes(session)).decode("ascii").rstrip("=").lower()
            self._authority.objects.add(self)
            _LIVE_SHMS.add(self)

    def _check(self) -> None:
        if self._pid != os.getpid(): raise RuntimeError("inherited mapping authority: spawn/exec required")
        self._authority.check()
        if not self._ptr: raise RuntimeError("shared-memory generation is closed")

    def grant(self, endpoint_index: int, pid: int | None = None) -> dict[str, Any]:
        """Issue once, after registration. Deliver only over a trusted control path."""
        _uint(endpoint_index, 32, "endpoint_index")
        if endpoint_index >= self.producers + self.consumers: raise ValueError("invalid endpoint index")
        pid = os.getpid() if pid is None else pid
        if not isinstance(pid, int) or isinstance(pid, bool) or not 0 < pid < 1 << 31: raise ValueError("invalid pid")
        with self._authority.lock:
            self._check()
            if endpoint_index in self._issued: raise ValueError("endpoint grant is one-shot")
            _check(N.lib.elite_object_register_process(self._ptr, endpoint_index, pid), "register_process", self)
            g = N.Grant()
            _check(N.lib.elite_object_grant(self._ptr, endpoint_index, C.byref(g)), "grant", self)
            self._issued[endpoint_index] = pid
            return _encode(g)

    def producer(self, index: int = 0) -> _Endpoint:
        if not 0 <= index < self.producers: raise ValueError("invalid producer index")
        cls = EliteSpscProducer if self.mode == "spsc" else EliteNcqProducer
        endpoint = cls(self.grant(index), _owner=self)
        self._local.add(endpoint)
        return endpoint

    def consumer(self, index: int = 0) -> _Endpoint:
        if not 0 <= index < self.consumers: raise ValueError("invalid consumer index")
        cls = EliteSpscConsumer if self.mode == "spsc" else EliteNcqConsumer
        endpoint = cls(self.grant(self.producers + index), _owner=self)
        self._local.add(endpoint)
        return endpoint

    def ack_cleanup(self, receipt: Mapping[str, Any]) -> None:
        """Accept a trusted endpoint's complete cleanup receipt; not proof by PID."""
        native = _decode(N.Receipt, receipt)
        with self._authority.lock:
            self._check()
            _check(N.lib.elite_object_ack_cleanup(self._ptr, C.byref(native)), "ack_cleanup", self)

    def reap(self, child_pid: int, timeout: float = 0.0) -> int | None:
        """Reap a registered owned child through the native authority.

        For UNRESOLVED grants, do not call Popen.wait/poll first. A terminal
        result retires the affected generation. If every grant already has an
        acknowledged cleanup receipt, use normal Popen.wait instead; the native
        authority no longer holds an unresolved child registration.
        Return terminal wait status or None. SIGSTOP is not terminal. Fencing
        resolves mapping-holder lifetime, never a guessed lost-token return.
        """
        if not isinstance(timeout, (int, float)) or not 0 <= timeout <= 86400:
            raise ValueError("timeout must be finite and in [0,86400]")
        if child_pid not in self._issued.values() or child_pid == self._pid: raise ValueError("not a registered owned child")
        end = time.monotonic() + timeout
        while True:
            with self._authority.lock:
                self._check(); status = C.c_int()
                r = N.lib.elite_authority_reap_child(self._authority.ptr, child_pid, C.byref(status))
                if r.status == 0: return status.value
                if r.status != 8: _check(r, "reap_child", self)
            if time.monotonic() >= end: return None
            time.sleep(min(0.001, max(0.0, end - time.monotonic())))

    def retire(self, reason: int = 0) -> None:
        with self._authority.lock:
            self._check(); _check(N.lib.elite_object_retire(self._ptr, _uint(reason, 64, "reason")), "retire", self)

    def successor(self) -> EliteShm:
        """Retire/quarantine this generation, create distinct backing in SAME budget."""
        with self._authority.lock:
            self._check(); self.retire(3)
            _check(N.lib.elite_object_quarantine(self._ptr), "quarantine", self)
            return EliteShm(self.mode, self.capacity, self.max_payload, self.producers, self.consumers,
                            self.checksum, self.parkable, self._authority.max_backing_bytes,
                            _authority=self._authority)

    def close(self) -> None:
        if not self._ptr: return
        # Never hold authority lock while acquiring endpoint locks: endpoint close
        # acknowledges in endpoint->authority order.
        self._check()
        endpoints = list(self._local)
        for endpoint in endpoints:
            if endpoint.busy: raise BusyError(N.Result(4, 5, 0, 0), "shm.close: outstanding lease/view", endpoint)
        for endpoint in endpoints: endpoint.close()
        with self._authority.lock:
            self._check()
            self._life.close()
            self._authority.objects.discard(self)

    def __enter__(self): self._check(); return self
    def __exit__(self, exc_type, exc, tb): self.close(); return False


class _Endpoint:
    _role: int
    _profile: int

    def __init__(self, grant: Mapping[str, Any], *, _owner: EliteShm | None = None):
        self._pid = os.getpid(); self._lock = threading.RLock(); self._ptr = N.P()
        self._pin: weakref.ReferenceType[_Pin] | None = None
        self._owner = _owner; self._receipt = None
        self._life = _EndpointLifetime(self._ptr, self._lock, self._pid,
                                       _owner._life if _owner is not None else None)
        self._finalizer = weakref.finalize(self, self._life.request_close)
        native = _decode(N.Grant, grant)
        if native.role != self._role or native.layout_profile != self._profile: raise ValueError("grant role/profile mismatch")
        r = N.lib.elite_attach(C.byref(native), C.byref(self._ptr))
        if r.status:
            # Preserve even cleanup-only or partially enrolled handles. Caller
            # obtains it as exception.resource and must acknowledge its receipt.
            raise EliteError(r, "attach", self)
        n, b, p, session = N.U64(), N.U32(), N.U32(), N.ID()
        _check(N.lib.elite_binding_info(self._ptr, None, C.byref(n), C.byref(b), C.byref(p), session), "get_info", self)
        self.capacity, self.max_payload = n.value, b.value
        self.session_id = bytes(session).hex()

    def _check(self) -> None:
        if self._pid != os.getpid(): raise RuntimeError("inherited endpoint: use spawn/exec and a constructed grant")
        if not self._ptr: raise RuntimeError("endpoint is closed")

    @property
    def busy(self) -> bool:
        with self._lock:
            return self._life.busy

    def close(self) -> dict[str, Any] | None:
        with self._lock:
            self._receipt = self._life.close()
            return self._receipt

    def _obtain(self, writing: bool):
        with self._lock:
            self._check()
            if self.busy: raise BusyError(N.Result(4, 5, 0, 0), "one outstanding token per endpoint", self)
            lease = N.Lease(); span = N.WriteSpan() if writing else N.ReadSpan()
            state = _LeaseState(self._life, lease, span, writing)
            # Install recovery ownership BEFORE ctypes may acquire a token.
            self._life._pin = weakref.ref(state)
            pin = _Pin(self, state)
            self._pin = weakref.ref(pin)
            resource = WriteLease(pin) if writing else ReadLease(pin)
            f = N.lib.elite_write_reserve if writing else N.lib.elite_read_borrow
            try:
                state.acquiring = True
                r = f(self._ptr, C.byref(lease), C.byref(span))
                state.adopt(span)
                state.acquiring = False
            except BaseException:
                state.acquiring = False
                state.uncertain = True
                state.active = bool(any(lease.opaque))
                self._life.poisoned = True
                _retain_failure(state, EliteError(N.Result(12, 6, 0, 0), "interrupted acquisition: outcome uncertain", state))
                raise
            if r.status in (1, 2):
                self._life._pin = None
                return None
            if r.status and not any(lease.opaque):
                self._life._pin = None
                _check(r, "reserve" if writing else "borrow", self)
            if r.status: raise EliteError(r, "lease acquisition retained ownership", resource)
            return resource

    def _wait_obtain(self, writing: bool, timeout: float):
        if not isinstance(timeout, (float, int)) or not 0 <= timeout <= 86400: raise ValueError("timeout must be finite and in [0,86400]")
        end = time.monotonic() + timeout
        while True:
            lease = self._obtain(writing)
            if lease is not None: return lease
            if time.monotonic() >= end:
                raise WouldBlock(N.Result(2 if writing else 1, 0, 0, 0), "availability timeout")
            # Python-level timed polling is separate from native parkable mode.
            time.sleep(0)

    def enable_drain(self) -> None:
        with self._lock:
            self._check(); _check(N.lib.elite_enable_drain(self._ptr), "enable_drain", self)

    def __enter__(self): self._check(); return self
    def __exit__(self, exc_type, exc, tb): self.close(); return False


class _LeaseState:
    """Cleanup data with no path back to any application wrapper/exporter.

    A weakref finalizer roots this state until _Pin dies. The exporter also
    retains it directly, so cyclic GC may clear _Pin/endpoint dictionaries in
    any order without destroying native cleanup metadata.
    """
    def __init__(self, endpoint: _EndpointLifetime, lease: N.Lease, span, writing: bool):
        self.endpoint, self.lease, self.writing = endpoint, lease, writing
        self.address = span.data
        self.length = span.capacity if writing else span.length
        self.message_type = 0 if writing else span.message_type
        self.message_id = 0 if writing else span.message_id
        self.epoch = int(lease.opaque[5])
        self.active = False; self.view_pin = False; self.uncertain = False
        self.acquiring = False; self.cleanup_requested = False

    def adopt(self, span):
        self.active = bool(any(self.lease.opaque))
        self.address = span.data
        self.length = span.capacity if self.writing else span.length
        self.message_type = 0 if self.writing else span.message_type
        self.message_id = 0 if self.writing else span.message_id
        self.epoch = int(self.lease.opaque[5])

    def _exporter_cleanup_failed(self):
        _retain_failure(self, RuntimeError("exporter finalization did not complete native unpin"))

    def _assert_live(self) -> None:
        self.endpoint._check()
        if not self.active: raise BufferError("lease ownership has ended")
        if self.uncertain: raise BufferError("asynchronous transfer outcome is unknown; fence this endpoint process")

    def _retain_view_pin(self) -> None:
        with self.endpoint._lock:
            self._assert_live()
            if self.view_pin: raise RuntimeError("exporter already owns a native view pin")
            # Reserve the local pin transaction before a GIL-releasing FFI call.
            self.view_pin = True
            try:
                r = N.lib.elite_view_retain(self.endpoint._ptr, C.byref(self.lease))
            except BaseException as exc:
                self.uncertain = True
                self.endpoint.poisoned = True
                _retain_failure(self, exc)
                raise
            if r.status:
                self.view_pin = False
                _check(r, "view_retain", self)

    def _drop_view_pin(self) -> None:
        with self.endpoint._lock:
            if not self.view_pin: return
            self._assert_live()
            try:
                r = N.lib.elite_view_end(self.endpoint._ptr, C.byref(self.lease))
            except BaseException as exc:
                self.uncertain = True
                self.endpoint.poisoned = True
                _retain_failure(self, exc)
                raise
            if r.status:
                exc = EliteError(r, "view_end", self)
                _retain_failure(self, exc)
                raise exc
            self.view_pin = False
            if self.cleanup_requested: self.cleanup()

    def finish(self, action: str, length: int = 0, kind: int = 0, message_id: int = 0) -> N.Result:
        with self.endpoint._lock:
            self._assert_live()
            if self.view_pin: raise BusyError(N.Result(4, 5, 0, 0), "live payload exporter", self)
            try:
                if action == "commit":
                    r = N.lib.elite_write_commit(self.endpoint._ptr, C.byref(self.lease), length, kind, message_id)
                else:
                    f = {"abort": N.lib.elite_write_abort, "release": N.lib.elite_read_release,
                         "abandon": N.lib.elite_abandon_retained}[action]
                    r = f(self.endpoint._ptr, C.byref(self.lease))
            except BaseException as cause:
                # ctypes can deliver a Python signal after C has crossed an LP
                # but before its return value is assigned. Never re-export or
                # retry that token. A parent authority can fence this process.
                self.uncertain = True
                self.address = None
                exc = EliteError(N.Result(12, 6, 0, 0), "asynchronously interrupted transfer", self)
                _retain_failure(self, exc)
                raise exc from cause
            # Transfer is irrevocable even when the supplemental status is an error.
            if r.outcome in (2, 4) or (action == "abandon" and r.status == 0):
                self.active = False; self.address = None; self.endpoint._pin = None
                self.endpoint.try_close()
            _check(r, action, self)
            return r

    def request_cleanup(self):
        self.cleanup_requested = True
        self.cleanup()

    def cleanup(self):
        try:
            if self.endpoint._pid != os.getpid() or not self.active or self.uncertain: return
            # A collectible cycle can still contain a Py_buffer. Native unpin
            # must wait for bf_releasebuffer, never for an assumed GC order.
            if self.view_pin: return
            try:
                self.finish("abort" if self.writing else "release")
            except EliteError:
                if self.active:
                    _check(N.lib.elite_retire(self.endpoint._ptr, 0), "finalizer retirement", self)
                    self.finish("abandon")
        except BaseException as exc: _retain_failure(self, exc)


class _Pin:
    """Public graph retention plus separately rooted native cleanup data."""
    def __init__(self, endpoint, state):
        self.endpoint = endpoint
        self.state = state
        self._finalizer = weakref.finalize(self, state.request_cleanup)

    def __getattr__(self, name):
        return getattr(self.state, name)

    def _assert_live(self):
        self.state._assert_live()



class _Lease:
    def __init__(self, pin: _Pin):
        self._pin = pin
        self._exporter = None

    @property
    def buffer(self) -> memoryview:
        """New view directly into the slot. All derived views extend the borrow."""
        with self._pin.endpoint._lock:
            self._pin._assert_live()
            if self._exporter is None or self._exporter.closed:
                self._pin._retain_view_pin()
                try:
                    self._exporter = _elite_buffer.create(self._pin.address, self._pin.length,
                                                          not self._pin.writing, self._pin, self._pin.state)
                except BaseException:
                    self._pin._drop_view_pin(); raise
            return memoryview(self._exporter)

    @property
    def epoch(self) -> int: return self._pin.epoch
    @property
    def active(self) -> bool: return self._pin.active

    def _end_exports(self) -> None:
        if self._exporter is not None: self._exporter.close()

    def abandon(self) -> None:
        """Retired-generation disposal only; never re-enqueues an uncertain token."""
        with self._pin.endpoint._lock:
            self._end_exports(); self._pin.finish("abandon")

    def __enter__(self): self._pin._assert_live(); return self
    def __exit__(self, exc_type, exc, tb):
        if self.active:
            self.abort() if self._pin.writing else self.release()
        return False


class WriteLease(_Lease):
    @property
    def capacity(self) -> int: return self._pin.length

    def commit(self, length: int, message_type: int = 0, message_id: int = 0) -> None:
        """Publish after every writable alias ends. No automatic byte copy."""
        _uint(length, 32, "length"); _uint(message_type, 32, "message_type"); _uint(message_id, 64, "message_id")
        if length > self.capacity: raise ValueError("length exceeds lease capacity")
        with self._pin.endpoint._lock:
            self._end_exports(); self._pin.finish("commit", length, message_type, message_id)

    def abort(self) -> None:
        with self._pin.endpoint._lock:
            self._end_exports(); self._pin.finish("abort")


class ReadLease(_Lease):
    @property
    def length(self) -> int: return self._pin.length
    @property
    def message_type(self) -> int: return self._pin.message_type
    @property
    def message_id(self) -> int: return self._pin.message_id

    def release(self) -> None:
        with self._pin.endpoint._lock:
            self._end_exports(); self._pin.finish("release")

    def copy(self) -> bytes:
        """Explicit copying convenience; does not claim zero-copy."""
        with self.buffer as view: return bytes(view)


class _Producer(_Endpoint):
    _role = 1
    def try_reserve(self) -> WriteLease | None: return self._obtain(True)
    def reserve(self, timeout: float = 0.0) -> WriteLease: return self._wait_obtain(True, timeout)


class _Consumer(_Endpoint):
    _role = 2
    def try_borrow(self) -> ReadLease | None: return self._obtain(False)
    def borrow(self, timeout: float = 0.0) -> ReadLease: return self._wait_obtain(False, timeout)


class EliteSpscProducer(_Producer):
    """Single-producer endpoint constructed from one trusted SPSC grant."""
    _profile = 1


class EliteSpscConsumer(_Consumer):
    """Single-consumer endpoint; exported payload buffers are read-only."""
    _profile = 1


class EliteNcqProducer(_Producer):
    """Publish-first NCQ-SC64 producer with one outstanding token."""
    _profile = 2


class EliteNcqConsumer(_Consumer):
    """NCQ-SC64 work-sharing consumer with read-only borrowed buffers."""
    _profile = 2


def _normal_exit_cleanup() -> None:
    # Best effort only for idle/explicitly quiescent handles. Active views are
    # NOT force-released. An authority crash cannot run this callback.
    for shm in list(_LIVE_SHMS):
        if shm._pid == os.getpid() and shm._ptr:
            try: shm.close()
            except BaseException as exc: _retain_failure(shm, exc)

atexit.register(_normal_exit_cleanup)
