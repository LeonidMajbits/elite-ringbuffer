"""Finite ownership/control model with nondeterministic conservative snapshots.
Memory-order correctness is deliberately NOT inferred from SC interleaving;
formal/handoff.py checks the separate reads-from / happens-before obligation.
"""
from dataclasses import dataclass, replace
from models.ncq import put

@dataclass(frozen=True, slots=True)
class State:
    published:int
    reclaimed:int
    producer_pc:int
    consumer_pc:int
    cached_c:int
    cached_p:int
    left:int
    epochs:tuple
    word0:tuple
    word1:tuple
    committed:tuple
    owners:tuple
    read0:int=0
    producer_done:bool=False

class Model:
    name='spsc-control-ownership'
    def __init__(self,capacity=2,producers=1,consumers=1,operations=4,mutation='none',allow_abort=True):
        if capacity<2 or capacity&(capacity-1) or (producers,consumers)!=(1,1) or operations<1:
            raise ValueError('SPSC requires N power-of-two >=2 and 1P1C')
        if mutation not in ('none','publish_early'):raise ValueError('invalid SPSC mutation')
        self.n=capacity;self.m=operations;self.mutation=mutation;self.abort=allow_abort
        self.config=dict(capacity=capacity,reservations=operations,mutation=mutation,allow_abort=allow_abort,
            snapshots='every nondecreasing stale/current cursor within actual frontier',
            memory_scope='control-state overapproximation, not a C11 weak-memory simulation')
    def initial(self):
        n=self.n
        return State(0,0,0,0,0,0,self.m,(0,)*n,(0,)*n,(0,)*n,(0,)*n,(0,)*n)
    def terminal(self,s):return s.producer_done and s.consumer_pc==99
    def invariant(self,s):
        if not 0<=s.published-s.reclaimed<=self.n:return 'SPSC_CAPACITY',{}
        if not 0<=s.cached_c<=s.reclaimed or not 0<=s.cached_p<=s.published:return 'FUTURE_SNAPSHOT',{}
        for t in range(s.reclaimed,s.published):
            b=t%self.n
            if s.owners[b] not in (2,3):return 'SPSC_LIVE_TICKET_OWNERSHIP',{'ticket':t}
            if s.word0[b]!=s.committed[b] or s.word1[b]!=s.committed[b]:
                return 'SPSC_TORN_OR_UNCOMMITTED',{'ticket':t}
        return None
    def successors(self,s):
        b=s.published%self.n;pc=s.producer_pc
        if not s.producer_done:
            if pc==0:
                if s.left==0:
                    yield 'P: finite budget complete',replace(s,producer_done=True),True,None
                elif s.published-s.cached_c>=self.n:
                    for c in range(s.cached_c,s.reclaimed+1):
                        yield f'P: acquire reclaimed snapshot {c}',replace(s,cached_c=c,producer_pc=1),False,None
                else:
                    yield 'P: cached reclamation covers reserve',replace(s,producer_pc=2),False,None
            elif pc==1:
                if s.published-s.cached_c>=self.n:
                    yield 'P: NO_CAPACITY_OBSERVED',replace(s,producer_pc=0),True,None
                else:yield 'P: capacity available',replace(s,producer_pc=2),False,None
            elif pc==2:
                if s.owners[b]!=0:
                    yield 'P: reserve conflict',s,False,('OVERWRITE_LIVE_LEASE',{'block':b});return
                yield 'P: private reservation and epoch advance',replace(s,producer_pc=3,left=s.left-1,
                    owners=put(s.owners,b,1),epochs=put(s.epochs,b,s.epochs[b]+1)),False,None
            elif pc in (3,4,5):
                if self.abort:
                    yield 'P: abort, no publication',replace(s,producer_pc=0,owners=put(s.owners,b,0)),True,None
                value=self.m-s.left
                if pc==3:
                    yield 'P: ordinary word0 write',replace(s,producer_pc=4,word0=put(s.word0,b,value)),False,None
                elif pc==4:
                    yield 'P: ordinary word1 write',replace(s,producer_pc=5,word1=put(s.word1,b,value)),False,None
                    if self.mutation=='publish_early':
                        yield 'P: MUTANT publication before second word',replace(s,published=s.published+1,producer_pc=6,
                            committed=put(s.committed,b,value),owners=put(s.owners,b,2)),True,None
                else:
                    yield 'P: release published [LP]',replace(s,published=s.published+1,producer_pc=6,
                        committed=put(s.committed,b,value),owners=put(s.owners,b,2)),True,None
            elif pc==6:
                yield 'P: local-only post-LP completion',replace(s,producer_pc=0),True,None
        b=s.reclaimed%self.n;pc=s.consumer_pc
        if pc==0:
            if s.cached_p<=s.reclaimed:
                for p in range(s.cached_p,s.published+1):
                    yield f'C: acquire publication snapshot {p}',replace(s,cached_p=p,consumer_pc=1),False,None
            else:yield 'C: covering cached publication',replace(s,consumer_pc=2),False,None
        elif pc==1:
            if s.cached_p<=s.reclaimed:
                # Termination is a test-harness decision after producer quiescence,
                # not a release/acquire edge supplied to the payload litmus.
                done=s.producer_done and s.reclaimed==s.published
                yield 'C: NO_DATA_OBSERVED',replace(s,consumer_pc=99 if done else 0),True,None
            else:yield 'C: covered publication',replace(s,consumer_pc=2),False,None
        elif pc==2:
            if s.owners[b]!=2:
                yield 'C: invalid borrow',s,False,('READ_UNCOMMITTED',{'block':b});return
            yield 'C: borrow holds C cursor',replace(s,consumer_pc=3,owners=put(s.owners,b,3)),False,None
        elif pc==3:
            yield 'C: ordinary word0 read',replace(s,consumer_pc=4,read0=s.word0[b]),False,None
        elif pc==4:
            if s.word1[b]!=s.read0:
                yield 'C: ordinary word1 read',s,False,('TORN_PAYLOAD',{'block':b});return
            yield 'C: final ordinary read',replace(s,consumer_pc=5),False,None
        elif pc==5:
            yield 'C: release reclaimed [LP]',replace(s,reclaimed=s.reclaimed+1,consumer_pc=6,
                owners=put(s.owners,b,0),read0=0),True,None
        elif pc==6:
            yield 'C: local-only post-LP completion',replace(s,consumer_pc=0),True,None
