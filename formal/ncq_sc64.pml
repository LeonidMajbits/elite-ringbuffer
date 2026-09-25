/* OPTIONAL Spin model. Exact SC index queues, finite one-publication workload.
 * Atomic blocks cover a single modeled CAS plus ghost bookkeeping. No FAA.
 * Run each LTL claim by name. See formal/README.md for scope and commands.
 * This file is independently handwritten; not automatically source-extracted.
 */
#define N 4
#define P 2
#define C 2
#define K 4
typedef Queue { byte h; byte t; byte u; byte e[N]; }
Queue q[2];
byte owner[N]; /* ghost: 0 QF; 1 QR; endpoint id+2 privately owns */
byte phase[N],epoch[N],x[N],y[N];
byte pdone=0,done=0;
byte owned[K]; /* zero=none; otherwise token+1, ghost only */
ltl frontier { [] ((q[0].h<=q[0].u)&&(q[1].h<=q[1].u)&&(q[0].t<=q[0].u)&&(q[1].t<=q[1].u)&&(q[0].t+1>=q[0].u)&&(q[1].t+1>=q[1].u)) }
ltl mutual_exclusion { [] (
 (owned[0]==0 || owned[1]==0 || owned[0]!=owned[1]) &&
 (owned[0]==0 || owned[2]==0 || owned[0]!=owned[2]) &&
 (owned[0]==0 || owned[3]==0 || owned[0]!=owned[3]) &&
 (owned[1]==0 || owned[2]==0 || owned[1]!=owned[2]) &&
 (owned[1]==0 || owned[3]==0 || owned[1]!=owned[3]) &&
 (owned[2]==0 || owned[3]==0 || owned[2]!=owned[3])) }
ltl termination { <> (done==K) }
inline dequeue(which,id,block,ok,h,e){
 ok=0;
 do
 :: h=q[which].h;
    e=q[which].e[h%N];
    if
    :: e/N==h/N ->
       atomic {
         if
         :: q[which].h==h ->
            block=e%N;assert(owner[block]==which);
            assert(owned[id]==0);owned[id]=block+1;
            q[which].h=h+1;owner[block]=id+2;ok=1
         :: else -> skip
         fi
       };
       if :: ok -> break :: else -> skip fi
    :: (h>=N)&&(e/N+1==h/N) -> break
    :: else -> skip
    fi
 od
}
inline enqueue(which,id,block,t,e,ok){
 ok=0;
 do
 :: t=q[which].t;e=q[which].e[t%N];
    if
    :: e/N==t/N ->
       atomic {if :: q[which].t==t -> q[which].t=t+1 :: else -> skip fi}
    :: (t>=N)&&(e/N+1==t/N) ->
       atomic {
         if
         :: q[which].e[t%N]==e ->
            assert(t==q[which].u);assert(q[which].u-q[which].h<N);
            assert(owner[block]==id+2);
            q[which].e[t%N]=(t/N)*N+block;
            q[which].u=t+1;owner[block]=which;owned[id]=0;ok=1
         :: else -> skip
         fi
       };
       if :: ok -> break :: else -> skip fi
    :: else -> skip
    fi
 od;
 atomic {if :: q[which].t==t -> q[which].t=t+1 :: else -> skip fi}
}
proctype Worker(byte id){
 byte b,h,t,e;bool ok;bool draining=false;
 if
 :: id<P ->
    do :: dequeue(0,id,b,ok,h,e);if :: ok -> break :: else -> skip fi od;
    assert(phase[b]==0);epoch[b]++;phase[b]=1;
    x[b]=id+1;y[b]=id+1;phase[b]=2;
    enqueue(1,id,b,t,e,ok);
    atomic {pdone++}
 :: else ->
    do
    :: if :: pdone==P -> draining=true :: else -> skip fi;
       /* Observe completion BEFORE the final empty check. */
       dequeue(1,id,b,ok,h,e);
       if
       :: ok ->
          assert(phase[b]==2);phase[b]=3;
          assert(x[b]==y[b]);phase[b]=0;
          enqueue(0,id,b,t,e,ok)
       :: else -> if :: draining -> break :: else -> skip fi
       fi
    od
 fi;
 atomic {
   assert(owned[id]==0);done++;
   if :: done==K -> assert(q[0].u-q[0].h==N);assert(q[1].u==q[1].h)
      :: else -> skip fi
 }
}
init {
 byte i=0;
 atomic {
 q[0].h=0;q[0].t=N;q[0].u=N;q[1].h=N;q[1].t=N;q[1].u=N;
 do :: i<N -> q[0].e[i]=i;q[1].e[i]=0;owner[i]=0;i++ :: else -> break od;
 i=0;do :: i<K -> run Worker(i);i++ :: else -> break od
 }
}
