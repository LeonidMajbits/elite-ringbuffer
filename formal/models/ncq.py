"""NCQ-SC64 finite transition system; no native source is replaced.

One transition contains at most one shared queue atomic operation, plus local
work and observational ghost updates. Payload words and phase stores are separate
steps. SC interleaving is appropriate ONLY for the frozen SC queue metadata;
C11 payload synchronization is checked separately by handoff.py.
"""
from dataclasses import dataclass, replace


@dataclass(frozen=True, slots=True)
class Actor:
    pc: int = 0
    ticket: int = 0
    entry: int = 0
    block: int = -1
    epoch: int = 0
    left: int = 1
    owns: bool = False
    queue: int = 0
    alias: bool = False
    read: int = 0


@dataclass(frozen=True, slots=True)
class State:
    heads: tuple
    tails: tuple
    frontiers: tuple
    entries: tuple
    epochs: tuple
    phases: tuple
    status_epochs: tuple
    word0: tuple
    word1: tuple
    owners: tuple
    actors: tuple


def put(items, index, value):
    return items[:index] + (value,) + items[index+1:]


class Model:
    name = 'ncq-sc64'

    def __init__(self, capacity=4, producers=2, consumers=2, operations=1,
                 mutation='none', allow_abort=True):
        if capacity < 2 or capacity & (capacity-1) or producers < 1 or consumers < 1:
            raise ValueError('invalid finite model configuration')
        if producers+consumers > capacity or operations < 1:
            raise ValueError('K <= N and positive finite reservation budget required')
        self.n, self.p, self.c, self.m = capacity, producers, consumers, operations
        if mutation not in {'none','split_head','stale_head_retry','overwrite_current','tail_before_entry','post_lp_write','early_return'}:
            raise ValueError('invalid NCQ mutation')
        self.mutation, self.abort = mutation, allow_abort
        self.limit = (1 << 64)-capacity-1
        self.config = dict(capacity=capacity, producers=producers, consumers=consumers,
                           reservations_per_producer=operations, mutation=mutation,
                           allow_abort=allow_abort, atomic_model='SC',
                           initialization='QF full; QR empty; construction precedes exposure')

    def initial(self):
        n=self.n
        return State((0,n),(n,n),(n,n),tuple(range(n))+(0,)*n,
                     (0,)*n,(0,)*n,(0,)*n,(0,)*n,(0,)*n,(-1,)*n,
                     tuple(Actor(left=self.m) for _ in range(self.p+self.c)))

    def invariant(self,s):
        n=self.n
        live=[[],[]]
        for q in (0,1):
            h,t,u=s.heads[q],s.tails[q],s.frontiers[q]
            if not (0 <= h <= u and 0 <= u-h <= n and u-1 <= t <= u):
                return 'QUEUE_FRONTIER_ORDER', {'queue':q,'head':h,'tail':t,'U':u}
            for ticket in range(h,u):
                entry=s.entries[q*n+ticket%n]
                if entry//n != ticket//n:
                    return 'PUBLICATION_HOLE', {'queue':q,'ticket':ticket,'entry':entry}
                b=entry%n
                if b in live[q]:return 'DUPLICATE_QUEUE_TOKEN', {'queue':q,'block':b}
                live[q].append(b)
                if s.owners[b] != -1-q:
                    return 'TOKEN_MEMBERSHIP', {'queue':q,'block':b,'owner':s.owners[b]}
                if s.phases[b] != (0 if q==0 else 2) or s.status_epochs[b]!=s.epochs[b]:
                    return 'QUEUED_PHASE', {'queue':q,'block':b}
                if q==1 and s.word0[b]!=s.word1[b]:
                    return 'UNFINISHED_PAYLOAD_PUBLISHED', {'block':b}
        if set(live[0]) & set(live[1]):return 'TOKEN_IN_BOTH_QUEUES', {}
        seen=live[0]+live[1]
        for i,a in enumerate(s.actors):
            if a.owns:
                if a.block<0 or s.owners[a.block]!=i:
                    return 'DOUBLE_ALLOCATION', {'actor':i,'block':a.block}
                seen.append(a.block)
            if a.alias and (not a.owns or s.owners[a.block]!=i):
                return 'RECLAIM_WITH_LIVE_ALIAS', {'actor':i,'block':a.block}
        if sorted(seen)!=list(range(n)):
            return 'TOKEN_CONSERVATION', {'live_multiset':seen}
        if any(x<0 for x in s.epochs):return 'EPOCH_RANGE',{}
        return None

    def terminal(self,s):
        return all(a.pc==99 for a in s.actors)

    def successors(self,s):
        n=self.n
        for i,a in enumerate(s.actors):
            if a.pc==99:continue
            producer=i<self.p
            q=0 if producer else 1
            kw={}; b=a.block; event=''; progress=False
            new=a
            if a.pc==0:
                if producer and a.left==0:
                    new=Actor(pc=99,left=0);event='producer finished finite budget'
                else:
                    new=replace(a,pc=1,ticket=0,entry=0,block=-1,epoch=0,
                                owns=False,queue=q,alias=False,read=0)
                    event=f'begin dequeue Q{q}'
            elif a.pc==1:
                new=replace(a,pc=2,ticket=s.heads[q]);event=f'load Q{q}.head'
            elif a.pc==2:
                e=s.entries[q*n+a.ticket%n]; ec=e//n; tc=a.ticket//n
                if ec==tc:
                    new=replace(a,pc=4,entry=e);event=f'load Q{q}.entry current generation'
                elif tc>0 and ec==tc-1:
                    done=(not producer and all(x.pc==99 for x in s.actors[:self.p]))
                    new=replace(a,pc=99 if done else 0,ticket=0,entry=0)
                    event=f'Q{q} empty observation / return';progress=True
                else:
                    new=replace(a,pc=1,ticket=0,entry=0);event=f'Q{q} stale generation retry'
            elif a.pc==4:
                h=s.heads[q]
                if h==a.ticket or self.mutation=='split_head':
                    b=a.entry%n
                    if s.owners[b]!=-1-q:
                        yield f'{i}: claim Q{q} ticket {a.ticket}',s,False,('DOUBLE_ALLOCATION',{'actor':i,'block':b,'previous_owner':s.owners[b]})
                        continue
                    kw['heads']=put(s.heads,q,a.ticket+1)
                    kw['owners']=put(s.owners,b,i)
                    new=replace(a,pc=5 if producer else 10,block=b,owns=True,ticket=0,entry=0)
                    event=f'Q{q} HEAD CAS success ticket {a.ticket}, token {b} [dequeue LP]';progress=True
                elif self.mutation=='stale_head_retry':
                    new=replace(a,ticket=h);event='MUTANT: refreshed head, retained stale entry'
                else:
                    new=replace(a,pc=1,ticket=0,entry=0);event='head CAS failure; discard observation set'
            elif a.pc==5:
                if s.phases[b]!=0 or s.status_epochs[b]!=s.epochs[b]:
                    yield f'{i}: validate free token',s,False,('INVALID_FREE_PHASE',{'block':b});continue
                g=s.epochs[b]+1
                kw['epochs']=put(s.epochs,b,g)
                new=replace(a,pc=6,epoch=g,left=a.left-1)
                event=f'owned epoch mirror {g}'
            elif a.pc==6:
                kw['phases']=put(s.phases,b,1);kw['status_epochs']=put(s.status_epochs,b,s.epochs[b]);new=replace(a,pc=7)
                event='release RESERVED'
            elif a.pc in (7,8):
                if self.abort:
                    abort=replace(s,phases=put(s.phases,b,0),actors=put(s.actors,i,replace(a,pc=20,queue=0)))
                    yield f'{i}: abort owned write; release EMPTY',abort,False,None
                value=1+i*self.m+(self.m-a.left-1)
                field='word0' if a.pc==7 else 'word1'
                kw[field]=put(getattr(s,field),b,value)
                new=replace(a,pc=a.pc+1);event=f'ordinary write {field}={value}'
            elif a.pc==9:
                kw['phases']=put(s.phases,b,2);new=replace(a,pc=20,queue=1)
                event='release COMMITTED; not queue membership'
            elif a.pc==10:
                if s.phases[b]!=2 or s.status_epochs[b]!=s.epochs[b] or s.word0[b]!=s.word1[b]:
                    yield f'{i}: validate claimed ready token',s,False,('TORN_OR_UNCOMMITTED',{'block':b});continue
                kw['phases']=put(s.phases,b,3);new=replace(a,pc=11,epoch=s.epochs[b],alias=True)
                event='owned metadata validation; CONSUMED; alias starts'
            elif a.pc==11:
                new=replace(a,pc=12,read=s.word0[b]);event='ordinary read word0'
                if self.mutation=='early_return':
                    kw['phases']=put(s.phases,b,0);new=replace(new,pc=20,queue=0)
                    event='MUTANT: return while read alias remains'
            elif a.pc==12:
                if s.word1[b]!=a.read:
                    yield f'{i}: read word1',s,False,('TORN_PAYLOAD',{'block':b});continue
                new=replace(a,pc=13,alias=False,read=0);event='ordinary read word1; final alias ends'
            elif a.pc==13:
                kw['phases']=put(s.phases,b,0);new=replace(a,pc=20,queue=0)
                event='release EMPTY; not QF membership'
            elif a.pc==20:
                q=a.queue;t=s.tails[q]
                if t>=self.limit:raise RuntimeError('finite run unexpectedly reached native ticket ceiling')
                new=replace(a,pc=21,ticket=t,entry=0);event=f'load Q{q}.tail'
            elif a.pc==21:
                q=a.queue;e=s.entries[q*n+a.ticket%n]
                if e//n==a.ticket//n:
                    new=replace(a,pc=25,entry=e);event='current entry; choose tail helping'
                elif a.ticket>=n and e//n==a.ticket//n-1:
                    new=replace(a,pc=22,entry=e);event='one-older entry; prepare install'
                    if self.mutation=='tail_before_entry':
                        kw['tails']=put(s.tails,q,a.ticket+1);event='MUTANT: advance tail before installation'
                else:
                    new=replace(a,pc=20,ticket=0,entry=0);event='stale enqueue observation'
            elif a.pc==22:
                q=a.queue;pos=q*n+a.ticket%n;e=s.entries[pos]
                if e==a.entry:
                    if not a.owns or s.owners[b]!=i:
                        yield f'{i}: install without token',s,False,('INSTALL_WITHOUT_OWNERSHIP',{});continue
                    if a.ticket!=s.frontiers[q]:
                        yield f'{i}: installation at {a.ticket}',s,False,('OUT_OF_ORDER_PUBLICATION',{'ticket':a.ticket,'U':s.frontiers[q]});continue
                    kw['entries']=put(s.entries,pos,(a.ticket//n)*n+b)
                    kw['frontiers']=put(s.frontiers,q,a.ticket+1)
                    kw['owners']=put(s.owners,b,-1-q)
                    new=replace(a,pc=23,owns=False);progress=True
                    event=f'Q{q} ENTRY CAS success ticket {a.ticket}, token {b} [publication LP]'
                elif self.mutation=='overwrite_current':
                    new=replace(a,entry=e);event='MUTANT: adopt winner entry as replacement expected'
                else:
                    new=replace(a,pc=20,ticket=0,entry=0);event='entry CAS failure; discard observation set'
            elif a.pc in (23,25):
                q=a.queue
                if self.mutation=='post_lp_write' and a.pc==23:
                    yield f'{i}: MUTANT descriptor write after publication',s,False,('POST_LP_PAYLOAD_ACCESS',{'block':b});continue
                if s.tails[q]==a.ticket:kw['tails']=put(s.tails,q,a.ticket+1)
                new=replace(a,pc=24 if a.pc==23 else 20,ticket=0,entry=0)
                event=f'Q{q} tail CAS; {"return tail" if a.pc==23 else "helper retry"}'
            elif a.pc==24:
                new=Actor(pc=0,left=a.left);event='enqueue response; no transferred-block access';progress=True
            else:raise RuntimeError(f'unknown pc {a.pc}')
            if a.owns and s.owners[a.block]!=i:
                yield f'{i}: before owned access',s,False,('OWNER_MISMATCH',{});continue
            kw['actors']=put(s.actors,i,new)
            yield f'{i}: {event}',replace(s,**kw),progress,None
