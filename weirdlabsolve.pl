#!/usr/bin/perl -w

use strict;
$|++;

# solve the weird lab 3rd floor

# metrics and future solutions at the end of this file

my @third = ();
sub reset {
#   @third = ([0, 1, 1, 0],
#	     [1, 0, 0, 1],
#	     [1, 0, 0, 1],
#	     [0, 1, 1, 0]);
#   @third = ([0, 0, 0, 0],
#	     [0, 0, 0, 0],
#	     [0, 0, 0, 0],
#	     [0, 0, 0, 0]);
# fourth really
   @third = ([0, 0, 0, 0, 0],
	     [0, 1, 0, 1, 0],
	     [0, 1, 0, 1, 0],
	     [0, 0, 0, 0, 0]);
}
&reset;
# ok third success is all ones

# find sequence of flips to make all positions the same color (or dark?)
my $movelimit = 20;

# reckon we don't need to move any position more than once
# and that thus we only need at most 16 moves
#my @seq = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
my @seq = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
	   19, 20);
my ($width, $height, $size) = (5, 4, 20);
my %seenseq = ();
$seenseq{join(" ",@seq)}++;

&printthird;
my $successlength = 99;
my $outputcalls = 0;
my $OUTPUTCHECK = 1_000_000;	# how often to check output
my $ltime = time();
my $start = $ltime;

my $Debug = 0;

my @testseq = (
    [qw/0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19/],
    [qw/2 16 15 5 0 9 18 12 4 19 11 2 3 7 10 8 13 6 17 14/],
    [qw/17 4 5 3 16 14 18 19 1 7 9 13 12 6 0 11 10 15 2 8/],
    [qw/14 19 11 18 1 5 8 15 13 3 10 4 12 17 6 7 0 16 2 9/],
    [qw/11 13 15 2 5 16 1 12 7 3 18 14 8 9 19 0 17 4 10 6/]
    );
    # only needed for in-built testing

my $USAGE = "$0 [flags]
    -i		interactive mode, try to solve it by hand
		   m:	map
		   r:	reset
		   q:	exit
		0-19:	flip element (and surround) in selected spot
    -t		test mode.  show details of processing 5 sequences of 20
";

while (@ARGV) {
    $_ = shift;

    /^-(d+)/	&& do { $Debug+=length($1); next; };
    /^-i/	&& do { &interactive; exit(0); };
    /^-t/	&& do { &testmode; exit(0); };

    warn "Unknown argument $_\n$USAGE";
    exit 1;;
}

# the main functionality
&generate($size);

exit(0);
## END

sub interactive {
    &map;
    my $moves = 0;
    my @s = ();
    while (<STDIN>) {
	chomp;
	if ($_ eq "r") {
	    &reset; $moves=0; &printthird(" reset"); @s=();
	    next;
	}
	if ($_ eq "m") { &map; next; }
	next unless (($_ > -1) && ($_ < $size));
	push @s, $_;
	$moves++;
	&flip($_);
	&printthird(" moves: ".$moves);
	print "seq: ".join(", ",@s)."\n";
    }
    sub map {
    print
"Interactive Mode -- map:   0  1  2  3  4
			   5  6  7  8  9
			  10 11 12 13 14
			  15 16 17 18 19\n";
    }
}

sub testmode {
    $Debug=2;
    for (my $i=0; $i<(1+$#testseq); $i++) {
	print "---restart---\n";
	&printthird;
	@seq = @{$testseq[$i]};
	&output;
    }
}

## heap's algorithm will re-arrange @seq in each possible combination, calling
## &output on each one.
sub output {
    $outputcalls++;
    if (($outputcalls % $OUTPUTCHECK) == 0) {
	my $t = time();
	printf "$OUTPUTCHECK iterations in ".($t-$ltime)." sec ".
	       "[$outputcalls total, %d checks/sec]\n",
	       ($outputcalls / ($t-$start));
	$ltime=$t;
    }
    print "trying ".join(",",@seq)."\n" if ($Debug);
    for (my $s=0; $s<$size; $s++) {
	&flip($seq[$s]);
	if ($Debug > 1) {
	    print "flip $seq[$s]:\n";
	    &printthird;
	}
	if (&uniform & ($s+1 < $successlength)) {
	    # victory
	    print "\nsuccess in 1+$s with seq: ".join(", ",@seq[0..$s])."\n";
	    &printthird;
	    $successlength = $s + 1;
	    last;
	    # exit 0;
	    # ok the very first sequenced worked ... any others?

	}
    }
    &reset();
}

sub flip($) {
    # flip monster in position $x, which also flips those in all adjacent
    # positions
    # pos map:
    # 0 1 2 3
    # 4 5 6 7
    # 8 9 A B
    # C D E F
    # then
    # 0 1 2 3 4
    # 5 6 7 8 9
    # A B C D E
    # F G H I J
    my ($x, $y) = ($_[0] % $width, int($_[0] / $width));

    # flip position itself
    $third[$y]->[$x] = 0 + !($third[$y]->[$x]);

    # and all adjacent
    if ($y > 0) {
	$third[$y-1]->[$x] = 0 + !($third[$y-1]->[$x]);
    }
    if ($x > 0) {
	$third[$y]->[$x-1] = 0 + !($third[$y]->[$x-1]);
    }
    if ($y < ($height-1)) {
	$third[$y+1]->[$x] = 0 + !($third[$y+1]->[$x]);
    }
    if ($x < ($width-1)) {
	$third[$y]->[$x+1] = 0 + !($third[$y]->[$x+1]);
    }
}

sub uniform() {
    my $result = 0;
    for (my $y = 0; $y < $height; $y++) {
	for (my $x = 0; $x < $width; $x++) {
	    return 0 if ($third[$y]->[$x]);
#	    $result += $third[$y]->[$x];
	}
    }
    return 1;
#    if (($result == 0) or ($result == 16)) { }
#    if ($result == 16) {
#	return 1;
#    }
#    return 0;
}

sub printthird {
    $_[0] = "" unless (defined($_[0]));
    for (my $y = 0; $y < $height; $y++) {
	print "\t".join(" ",@{$third[$y]}).($y==($height-1)?$_[0]:"")."\n";
    }
}

## heap's algorithm to find all the permutations of 16 numbers...
# 
# procedure generate(n : integer, A : array of any):
#     if n = 1 then
#           output(A)
#     else
#         for i := 0; i < n - 1; i += 1 do
#             generate(n - 1, A)
#             if n is even then
#                 swap(A[i], A[n-1])
#             else
#                 swap(A[0], A[n-1])
#             end if
#         end for
#         generate(n - 1, A)
#     end if
# 
sub generate {
    # we already know the array is @seq
    my $n = shift @_;	# generate permutations of the first n elements of array

    if ($n == 1) {
	&output;
	return;
    }
    for (my $i = 0; $i < ($n-1); $i ++) {
	&generate($n - 1);
	if ($n % 2) {
	    # $n is odd, swap(A[0], A[n-1])
	    ($seq[0], $seq[$n-1]) = ($seq[$n-1], $seq[0]);
	} else {
	    # $n is even, swap(A[i], A[n-1])
	    ($seq[$i], $seq[$n-1]) = ($seq[$n-1], $seq[$i]);
	}
    }
    &generate($n-1);
}

# metrics
# speed rank: gir timair timmbp[c]|timvm[perl] timvm[c]|timmbp[p]
#	      timcent[c]|scripts[p] scripts[c]|timcent[p]
# timair:
# 1000000 iterations in 45 sec [5000000 total, 21834 checks/sec]
# timmbp:
# 1000000 iterations in 69 sec [5000000 total, 14367 checks/sec]
# scripts:
# 1000000 iterations in 92 sec [5000000 total, 10845 checks/sec]
# gir:
# 1000000 iterations in 42 sec [5000000 total, 23474 checks/sec]
# timvm:
# 1000000 iterations in 60 sec [5000000 total, 16556 checks/sec]
# timcent:
# 1000000 iterations in 95 sec [5000000 total, 10548 checks/sec]

# metrics for the C version
# timair:
# 10000000 iterations in 2 sec [200000000 total, 5.56 mil checks/sec] *254
# timmbp:
# 10000000 iterations in 2 sec [200000000 total, 4.88 mil checks/sec] *339
# scripts:
# 10000000 iterations in 5 sec [200000000 total, 2.74 mil checks/sec] *252
# gir:
# 10000000 iterations in 2 sec [200000000 total, 5.88 mil checks/sec] *250
# timvm:
# 10000000 iterations in 2 sec [200000000 total, 4.26 mil checks/sec] *257
# timcent:
#                                                3.02 mil checks/sec] *286
# 10000000 iterations in 4 sec [1380000000 total, 0.02 mil checks/sec]
# ...but not really, counter wrong
# % ps -o time -C a.out # 18:31:58
# % awk 'BEGIN {getline; gsub(/\[/,""); print $6" to start";t+=$6}
#        {t+=$1} END {print "total "t}' ~/screenlog.6
# total 201290000000
# % expr 201290000000 / \( 18 \* 3600 + 31 \* 60 \)
# 3019651
# 3.02 mil checks/sec

=cut
further steps:
. Update existing perl code to take a -test flag, print explicit steps and
  results for five test sequences
    0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19
    1 16 15 5 0 9 18 12 4 19 11 2 3 7 10 8 13 6 17 14
    17 4 5 3 16 14 18 19 1 7 9 13 12 6 0 11 10 15 2 8
    14 19 11 18 1 5 8 15 13 3 10 4 12 17 6 7 0 16 2 9
    11 13 15 2 5 16 1 12 7 3 18 14 8 9 19 0 17 4 10 6

. Update existing C code to take a -test flag, as for perl. Compare results
. Save existing C code aside

# revision 1.2
# checkpoint on timvm: 4.26 mil checks/sec

. New puzzle representation using single-dimensional array: 
    void resetpuzzle(int puzzle[HEIGHT][WIDTH]);
    void printpuzzle(int puzzle[HEIGHT][WIDTH]);
    void generate(int n, int *sequence, int puzzle[HEIGHT][WIDTH]);
    void flip(int puzzle[HEIGHT][WIDTH], int position);

    puzzle[HEIGHT*WIDTH]
    x+1, P+1 -- no for 4,9,14,19 P%WIDTH=WIDTH-1
    x-1, P-1 -- no for 0,5,10,15 P%WIDTH=0
    y+1, P+5 -- no for 15-19 P%WIDTH=HEIGHT-1
    y-1, P-5 -- no for 0-4 P%WIDTH=0

    ...no, use a LONG instead -- 20 bits

. Test the above

# revision 1.3
# checkpoint on timvm: 6.06 mil checks/sec [42% improvement over 4.26mil]

- New code takes mandatory arguments:
    -n[umsteps] N	:: examine all possible combinations with N steps
    ...it also tells you how many iterations are needed (N!) and estimates how
    long this will take based on average rate for C version (curr 4.39mil/sec)

- New permutation algorithm:
    int lexpermute(int *SET -- 
    inputs:
	int *SET -- set of possible values
	int *puzzle -- link to puzzle space in state we process on
	int *SOLUTIONPATH -- ordered set of values already applied to puzzle
	int STEPS -- number of steps of solution left to try
    code:
	int iterations=0;	# count the number we process internally
	int mypuzzle[PUZZLESIZE]

	if STEPS==1
	    # we have reached the end, check the result(s) & return
	    for each element in set
		iterations++
		copy puzzle to mypuzzle
		flip(mypuzzle, this-element)
		int solved=1
		for(i=0;i<PUZZLESIZE;i++) {
		    if mypuzzle[i]==0 then solved=0; break
		}
		if (solved)
		    # ok if we're here we've found a solution!
		    # print *SOLUTIONPATH and SET[0]
	    return iterations

	for each element in set
	    copy puzzle to mypuzzle
	    flip(mypuzzle, this-element)
	    int myset[sizeof(SET)-1];
	    copy all values of SET, except this element, into myset
	    int mysolutionpath[sizeof(SET)+1]
	    copy SOLUTIONPATH and this element into mysolutionpath
	    iterations += &lexpermute(mysolutionpath, puzzle, set, STEPS-1)
	
	return iterations

    ...and now startpoint can be any array, just process the steps in
    startpoint and call &lexpermute with that many fewer steps

#
# revision 1.4
# checkpoint on timvm: 92 mil checks/sec [15x improvement over 6.06mil]

    - use clock() for more precise timings, clean up timing a bit
    - make printing of sequences into a subroutine
    - add more debug code

#
# revision 1.5
#

# next we need to get average time to run lexpermute with diff sizes to select
# optimal thread length
typedef struct permtimes PERMTIMES;
struct permtimes {
    int steps;			/* this struct measures lexpermute calls with
				   this number of steps */
    double totalclocks;		/* total clock time for all calls measured */
    int calls;			/* number of times lexpermute called */
};
PERMTIMES perm_sizing[SIZE+1];	/* measure calls from 1-SIZE */
# after call
perm_sizing[steps]->totalclocks += clocks
perm_sizing[steps]->calls++;
perm_sizing[steps]->steps = steps;

pre-check:
./weirdlabsolve -d -d -n 4 > ~/Downloads/weirdout-1.5-n4.txt
# will this match when we re-arrange things and thread it?
# -n 8 and -n 7 had way too many lines...
verified for revision 1.6

    - re-arranged progress checking via outputcalls, as it will not work with
      threading -- now use permutecount and checkinterval.  Also move
      timecheck code inot a seperate subroutine for code clarity.
    - modify lexpermute() to take and return a void* in preparation for
      threading.  this includes setting up structure to pass to lexpermute()
      containing all the arguments.
    - add code to figure out average time to execute lexpermute with different
      step values.  fun but should be #defined out of 'production' code
    - cleaned out a lot of old unused code -- output(), generate(),
      bits of main() -- this means that testmode() modified to no longer use
      output()
    - new subroutine lexrun() to make main() cleaner to read
    - a couple additional uses of printseq() instead of longer code

#
# revision 1.6
# checkpoint on timvm: 25.63 mil checks/sec
# 	oh geez, slowing down by 1/3.7 -- from 93 mil checks/sec
# Elapsed 2737.3 seconds
# Timing per level:
# steps   1 average          1.2 clocks/run (  0.0 min/run)
# steps   2 average          6.3 clocks/run (  0.0 min/run)
# steps   3 average         91.3 clocks/run (  0.0 min/run)
# steps   4 average       1379.5 clocks/run (  0.0 min/run)
# steps   5 average      22167.0 clocks/run (  0.0 min/run)
# steps   6 average     379685.9 clocks/run (  0.4 min/run)
# steps   7 average    7273930.7 clocks/run (  7.3 min/run)
# steps   8 average  240836915.3 clocks/run ( 240.8 min/run)
# ...what if we #define out the extra timing code?
# ...that makes it around 80 mil checks/sec.  Still slower, but not so bad.
#
# right, so timecheck on timmbp:
# % time ( repeat 3 ./weirdlabsolve-Darwin1.5 -n 8 )
# v1.5; 91 mil checks/sec; 167.53s user   0.09s system 99% cpu  2:47.71 total
# v1.6; 14 mil checks/sec; 478.46s user 610.78s system 99% cpu 18:09.69 total
# v1.6.1; disable debug timing
#       67 mil checks/sec; 225.26s user   0.09s system 99% cpu  3:45.46 total
# v1.6.2; count iterations with register variable while in lexpermute()
#	64 mil checks/sec; 236.10s user   0.20s system 99% cpu  3:56.51 total
# v1.7: 39 mil checks/sec; 
#	what the hell? it's got near 100% cpu usage but...
#	-j 1 69 mil checks/sec; -j 2 65 mil checks/sec
revision 1.6.2
- hide away lexpermute timings with a #define, as it greatly slows things down
- experiment with using a register value to track iterations.  it doesn not
  help the speed
- add a third mainline debug level that prints the puzzle board as it works

# time to implement threads

- Make new permutation algo thread-ready?
    make it thread-ready by accumulating counters and things to print at level
    N-2?

    http://timmurphy.org/2010/05/04/pthreads-in-c-a-minimal-working-example/
    pseudocode:
	- if (steps == steps_to_thread_at) {
	    allocate THREADS pthread structures
	    push 0...THREADS-1 onto threadstack
	    complete[THREADS] = [0...0]
	    for each element of set
		if (threadstack[0] != -1) {
		    my thread = pop threadstack
		    # prepare next lexpermute call
		    start lexpermute in my thread
		}
		my freed=0
		foreach (thread marked complete) {
		    # we'd use pthread_join_np if linux-only
		    join thread
		    push thread onto threadstack
		    complete[thread] = 0
		    freed++
		    incremet global completed with thread results
		}
		if ((freed == 0) && (threadstack[0] == -1)) {
		    sleep 1
		}
	    }
    actually no easy way to tell what pthread id is from the parent, so just
    give up on the queuing thing for now.
	    
    in lexpermute,
	if i im a thread (pthread_self())
	- do not increment global completed var
	https://stackoverflow.com/questions/1130018/unix-portable-atomic-operations
	- use lock to update complete[THREADS] before returning
	https://stackoverflow.com/questions/10879420/using-of-shared-variable-by-10-pthreads
	https://www.cs.nmsu.edu/~jcook/Tools/pthreads/pthread_cond.html
	https://stackoverflow.com/questions/2156353/how-do-you-query-a-pthread-to-see-if-it-is-still-running
	https://stackoverflow.com/questions/3673208/pthreads-if-i-increment-a-global-from-two-different-threads-can-there-be-sync


    ./weirdlabsolve-Linux1.5 -d -d -n 4 | sort > /tmp/out1.5_lvl4_sort.txt
    ./weirdlabsolve -d -d -n 4 | sort > /tmp/out1.7_lvl4_sort.txt
    /bin/rm stderr.out /tmp/out1.7_lvl4_sort.txt; \
    ./weirdlabsolve -d -d -n 4 2>stderr.out | sort > /tmp/out1.7_lvl4_sort.txt
    # done, finally works!


- New code takes startpoint argument, with value between 0 and N!
  ...ok no i cannot find an easy O(1) formula for this, though I saw one in one
  of my other searches on stackoverflow.
    -s[tartpoint] S	:: only process sequences where the first element is S
    -s2			:: only process sequences where the first element is
			   S, and the second elemnt is S2

  Right, if we have S elements, and we are looking at N steps, then there are
  S * (S-1) * ... (S-N-1) possible permutations, and iteration X represents:
    element int (X/((S-1) * ... * (S-N-1))) of S,
    element int [remainder of above / ((S-2) * ... * (S-N-1))
	of the set without the previous element in it

  int factor[STEPS];		/* factor to use to go from iteration number
				   to position in moves array.  index is
				   number of steps remaining, so
				   factor[STEPS]   = SIZE-1*...*SIZE-STEPS+1
				   facotr[STEPS-1] = SIZE-2*...*SIZE-STEPS+1
				   ...
				*/
  int result=1, nextstep=SIZE-STEPS+1;
  for (int i=1; i <= STEPS; i++) {
    factor[i] = result;
    result = result * nextstep;
    nextstep++;
  }
  if (nextstep != SIZE) { warn "we got our math wrong\n"; exit(-1) }

  example: SIZE=4, STEPS=3			SIZE=6, STEPS=4
    total permutations: 4*3*2			6*5*4*3
			   factor[STEPSLEFT==4] = 5*4*3
    factor[STEPSLEFT==3] = 3*2			= 4*3
    factor[STEPSLEFT==2] = 2			= 3
    factor[STEPSLEFT==1] = 1			= 1

  example: SIZE=4, STEPS=3, moves: a b c d -- permutations 24
  iter#
     0 a b c [index 0 0 0]	 8 b c a [index 1 1 0]	16 c d a [index 2 2 0]
     1 a b d [index 0 0 1]	 9 b c d [index 1 1 1]  17 c d b        2 2 1
     2 a c b [index 0 1 0]	10 b d a [index 1 2 0]	18 d a b	3 0 0 
     3 a c d [index 0 1 1]	11 b d c [index 1 2 1]  19 d a c        3 0 1
     4 a d b [index 0 2 0]	12 c a b [index 2 0 0]  20 d b a	3 1 0
     5 a d d [index 0 2 1]	13 c a d	2 0 1	21 d b c	3 1 1
     6 b a c [index 1 0 0]	14 c b a	2 1 0	22 d c a	3 2 0
     7 b a d [index 1 0 1]	15 c b d	2 1 1   23 d c b	3 2 1		index: [iteration / 6, (iteration % 6)/2, (iteration % 6)%2]
    
  given STARTITERATION, compute array of starting positions
  int remainder=STARTITERATION;
  int STARTSTEP[STEPS-1]
  for (int i=0; i < STEPS; i++) {
    int stepsleft = SIZE-i;
    STARTSTEP[i] = remainder / factor[stepsleft];
    remainder = remainder % factor[stepsleft];
  }

  if no STARTITERATION, STARTSTEP[STEPS-1] = [all zeros]
  then, within lexpermute,
  when iterating over elements of (remaining moves array), we use
    for (i=STARTSTEP[stepsleft] ...)
  ..and when finished, we set STARTSTEP[stepsleft]=0;
    


https://stackoverflow.com/questions/5248915/execution-time-of-c-program

