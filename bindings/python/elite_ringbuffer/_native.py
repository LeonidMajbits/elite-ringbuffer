"""Private ctypes declarations for PROCESS-LOCAL structs, never shared atomics.
Every size/member offset is checked against the actually loaded C library.
"""
import ctypes as C
import os
import pathlib
import sys
import platform
import sysconfig

if platform.python_implementation() != "CPython" or sys.version_info < (3, 11):
    raise ImportError("ELITE requires CPython >= 3.11")
if sysconfig.get_config_var("Py_GIL_DISABLED") or not getattr(sys, "_is_gil_enabled", lambda: True)():
    raise ImportError("ELITE buffer exporter requires GIL-enabled CPython")
U32=C.c_uint32; U64=C.c_uint64; ID=C.c_uint8*16; P=C.c_void_p

class Result(C.Structure):
    _fields_ = [
        ("status", U32),
        ("outcome", U32),
        ("os_error", C.c_int32),
        ("reserved", U32),
    ]

class Lease(C.Structure):
    _fields_ = [
        ("opaque", U64*10),
    ]

class WriteSpan(C.Structure):
    _fields_ = [
        ("data", P),
        ("capacity", U32),
    ]

class ReadSpan(C.Structure):
    _fields_ = [
        ("data", P),
        ("length", U32),
        ("message_type", U32),
        ("message_id", U64),
        ("epoch", U64),
    ]

class Definition(C.Structure):
    _fields_ = [
        ("endpoint_id", ID),
        ("process_incarnation_id", ID),
        ("role", U32),
    ]

class Config(C.Structure):
    _fields_ = [
        ("layout_profile", U32),
        ("wait_mode", U32),
        ("payload_checksum_mode", U32),
        ("max_payload_bytes", U32),
        ("capacity", U64),
        ("producer_endpoints", U32),
        ("consumer_endpoints", U32),
        ("creation_utc_ns", U64),
    ]

class Grant(C.Structure):
    _fields_ = [
        ("name", C.c_char*31),
        ("constructed", U32),
        ("segment_bytes", U64),
        ("header_crc32", U32),
        ("layout_profile", U32),
        ("atomic_abi_id", U32),
        ("endpoint_index", U32),
        ("role", U32),
        ("authority_epoch", U64),
        ("grant_epoch", U64),
        ("session_id", ID),
        ("host_instance_id", ID),
        ("authority_instance_id", ID),
        ("endpoint_id", ID),
        ("process_incarnation_id", ID),
    ]

class Receipt(C.Structure):
    _fields_ = [
        ("session_id", ID),
        ("endpoint_id", ID),
        ("process_incarnation_id", ID),
        ("endpoint_index", U32),
        ("local_cleanup_complete", U32),
        ("owns_grant_claim", U32),
    ]

def _library_path():
    explicit = os.environ.get("ELITE_LIBRARY")
    if explicit:
        path = pathlib.Path(explicit).expanduser().resolve(strict=True)
        if not path.is_file(): raise ImportError("ELITE_LIBRARY is not a file")
        return path
    # Installed bundle locates an explicitly packaged native sibling. The source
    # workflow deliberately requires ELITE_LIBRARY so a stale system library
    # cannot be selected silently.
    path = pathlib.Path(__file__).resolve().parent / ("libelite_ringbuffer.dylib" if sys.platform == "darwin" else "libelite_ringbuffer.so")
    if not path.is_file():
        raise ImportError("Set ELITE_LIBRARY to the absolute built libelite_ringbuffer path; run make python first")
    return path

LIBRARY_PATH = _library_path()
lib = C.CDLL(str(LIBRARY_PATH), use_errno=True)
lib.elite_version_string.argtypes=[];lib.elite_version_string.restype=C.c_char_p
lib.elite_version_number.argtypes=[];lib.elite_version_number.restype=U32
if lib.elite_version_number() != 0x00010100:
    raise ImportError("native/binding release version mismatch")
lib.elite_local_layout.argtypes=[U32,U32];lib.elite_local_layout.restype=C.c_size_t
LOCAL_TYPES=(Result,Lease,WriteSpan,ReadSpan,Definition,Config,Grant,Receipt)
for _i,_type in enumerate(LOCAL_TYPES,1):
    if lib.elite_local_layout(_i,0xffffffff)!=C.sizeof(_type):
        raise ImportError(f"ctypes sizeof mismatch: {_type.__name__}")
    for _j,(_member,_ctype) in enumerate(_type._fields_):
        if lib.elite_local_layout(_i,_j)!=getattr(_type,_member).offset:
            raise ImportError(f"ctypes offset mismatch: {_type.__name__}.{_member}")

def _declare(name,*args):
    f=getattr(lib,name);f.argtypes=list(args);f.restype=Result

_declare("elite_authority_create",C.POINTER(C.c_uint8),C.POINTER(C.c_uint8),U64,C.POINTER(P))
_declare("elite_authority_destroy",C.POINTER(P))
_declare("elite_create",P,C.POINTER(Config),C.POINTER(Definition),C.POINTER(P))
_declare("elite_object_activate",P)
_declare("elite_object_register_process",P,U32,C.c_int)
_declare("elite_object_grant",P,U32,C.POINTER(Grant))
_declare("elite_object_ack_cleanup",P,C.POINTER(Receipt))
_declare("elite_authority_reap_child",P,C.c_int,C.POINTER(C.c_int))
_declare("elite_object_retire",P,U64)
_declare("elite_object_quarantine",P)
_declare("elite_object_destroy",C.POINTER(P))
_declare("elite_attach",C.POINTER(Grant),C.POINTER(P))
_declare("elite_write_reserve",P,C.POINTER(Lease),C.POINTER(WriteSpan))
_declare("elite_write_commit",P,C.POINTER(Lease),U32,U32,U64)
_declare("elite_write_abort",P,C.POINTER(Lease))
_declare("elite_read_borrow",P,C.POINTER(Lease),C.POINTER(ReadSpan))
_declare("elite_read_release",P,C.POINTER(Lease))
_declare("elite_view_retain",P,C.POINTER(Lease))
_declare("elite_view_end",P,C.POINTER(Lease))
_declare("elite_retire",P,U64)
_declare("elite_enable_drain",P)
_declare("elite_abandon_retained",P,C.POINTER(Lease))
_declare("elite_detach",C.POINTER(P),C.POINTER(Receipt))
_declare("elite_wait_data",P,U64)
_declare("elite_wait_space",P,U64)
_declare("elite_binding_info",P,P,C.POINTER(U64),C.POINTER(U32),C.POINTER(U32),C.POINTER(C.c_uint8))
lib.elite_status_string.argtypes=[U32];lib.elite_status_string.restype=C.c_char_p
