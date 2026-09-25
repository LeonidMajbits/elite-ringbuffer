/* OPTIONAL Spin input: SC ownership projection only, NOT C11 weak memory.
 * Run safety: spin -a spsc_ownership.pml; cc -O2 -DSAFETY -DNOCLAIM pan.c -o pan; ./pan
 * Run conditional termination separately: cc -O2 pan.c -o pan; ./pan -a -f
 * Weak fairness applies to thread execution; it does not set a time deadline.
 */
#define N 2
#define M 4
byte published=0,reclaimed=0;
byte x[N],y[N],owner[N];
byte done=0;
ltl capacity { [] ((published >= reclaimed) && (published-reclaimed <= N)) }
ltl eventually_done { <> (done == 2) }
active proctype Producer(){
 byte k=0;b: /* label has no synchronization semantics */
 do
 :: k<M ->
    (published-reclaimed<N);
    atomic { assert(owner[k%N]==0);owner[k%N]=1; }
    x[k%N]=k+1;
    y[k%N]=k+1;
    atomic { owner[k%N]=2;published=k+1; }
    k++
 :: else -> break
 od;
 atomic { done++ }
}
active proctype Consumer(){
 byte k=0;
 do
 :: k<M ->
    (published>k);
    atomic {assert(owner[k%N]==2);owner[k%N]=3;}
    assert(x[k%N]==k+1);
    assert(y[k%N]==k+1);
    atomic {owner[k%N]=0;reclaimed=k+1;}
    k++
 :: else -> break
 od;
 atomic {done++}
}
