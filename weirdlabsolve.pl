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
   @third = ([0, 0, 0, 0],
	     [0, 0, 0, 0],
	     [0, 0, 0, 0],
	     [0, 0, 0, 0]);
# fourth really
   @third = ([0, 0, 0, 0, 0],
	     [0, 0, 0, 0, 0],
	     [0, 1, 0, 1, 0],
	     [0, 0, 0, 0, 0]);
   # success in 1+7 with seq: 0, 2, 4, 6, 8, 10, 14, 17
   @third = ([0, 0, 1, 0],
	     [0, 0, 1, 1],
	     [1, 1, 0, 0],
	     [0, 1, 0, 0]);
   # success in 1+5 with seq: 0, 3, 6, 10, 13, 14
   @third = ([1, 0, 0, 0],
	     [0,  , 0, 0],
	     [0, 0,  , 0],
	     [0, 0, 0, 1]);
   # success in 1+3 with seq: 1, 4, 7, 13
   @third = ([0, 0, 0, 0, 0],
	     [0, 0, 0, 0, 0],
	     [1, 0, 1, 0, 1],
	     [0, 0, 0, 0, 0]);
# success in 1+13 with seq: 0, 1, 3, 4, 5, 6, 7, 8, 9, 11, 13, 16, 17, 18
   @third = ([0, 0, 0, 0, 0],
	     [0, 0, 0, 0, 0],
	     [0, 0, 0, 0, 0],
	     [1, 0, 0, 0, 1]);
# pos 7 is blank
# success in 1+13 with seq: 0, 1, 2, 3, 4, 5, 9, 10, 11, 13, 14, 16, 17, 18
}
&reset;
# ok third success is all ones

# find sequence of flips to make all positions the same color (or dark?)
my $movelimit = 20;

# reckon we don't need to move any position more than once
# and that thus we only need at most 16 moves
#my @seq = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
#my @seq = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
#	   19, 20);
#my ($width, $height, $size) = (5, 4, 20);
my ($width, $height) = (1+$#{$third[0]}, 1+$#third);
my $size = $width * $height;
my @seq = (0 .. ($size-1));
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
#&generate($size);
for (my $i=1; $i <= $size; $i++) {
    print "Trying size $i\n";
    &generate_new($i, 0, $size-1, []);
}

exit(0);
## END

## new algorithm -- order doesn't matter, so
## generate_new($size, $r_start, $r_end, $ra_sequence)
## test all combinations, in which the order is unimportant and each move is
## only relevant once, of size $size in the range $rstart - $rend.  if size=1,
## test the result of running the moves.
sub generate_new($$$$) {
    my ($size, $r_start, $r_end, $ra_sequence) = (@_);

    if ($size == 1) {
	for (my $j = $r_start; $j <= $r_end; $j++) {
	    @seq = (@$ra_sequence, $j);
	    &output;
	}
	return;
    }

    for (my $j = $r_start; $j <= $r_end; $j++) {
	&generate_new($size-1, $j+1, $r_end, [ @$ra_sequence, $j ]);
    }
    return;
}
    
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
"Interactive Mode -- map:   0  1  2  3  4  or  0  1  2  3
			   5  6  7  8  9      4  5  6  7
			  10 11 12 13 14      8  9 10 11
			  15 16 17 18 19     12 13 14 15\n";
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
    for (my $s=0; $s<=$#seq; $s++) {
	&flip($seq[$s]);
	if ($Debug > 1) {
	    print "flip $seq[$s]:\n";
	    &printthird("");
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

# sigh.  this has been the wrong way around. we want all 1s
sub uniform() {
    my $result = 0;
    for (my $y = 0; $y < $height; $y++) {
	for (my $x = 0; $x < $width; $x++) {
	    return 0 if ($third[$y]->[$x] == 0);
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
#
# START: Version 1.2
#
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

# checkpoint on timvm: 4.26 mil checks/sec
#
#   END: Version 1.2
# START: Version 1.3
#

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

#
# checkpoint on timvm: 6.06 mil checks/sec [42% improvement over 4.26mil]
#
#   END: Version 1.3
# START: Version 1.4
#

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
# checkpoint on timvm: 92 mil checks/sec [15x improvement over 6.06mil]
#
#   END: Version 1.4
# START: Version 1.5
#

    - use clock() for more precise timings, clean up timing a bit
    - make printing of sequences into a subroutine
    - add more debug code

#
#   END: Version 1.5
# START: Version 1.6
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
#   END: Version 1.6
# START: Version 1.6.2
#
# right, so timecheck on timmbp:
# % time ( repeat 3 ./weirdlabsolve-Darwin1.5 -n 8 )
# v1.5; 91 mil checks/sec; 167.53s user   0.09s system 99% cpu  2:47.71 total
# v1.6; 14 mil checks/sec; 478.46s user 610.78s system 99% cpu 18:09.69 total
# v1.6.1; disable debug timing
#       67 mil checks/sec; 225.26s user   0.09s system 99% cpu  3:45.46 total
# v1.6.2; count iterations with register variable while in lexpermute()
#	64 mil checks/sec; 236.10s user   0.20s system 99% cpu  3:56.51 total
revision 1.6.2
- hide away lexpermute timings with a #define, as it greatly slows things down
- experiment with using a register value to track iterations.  it doesn not
  help the speed
- add a third mainline debug level that prints the puzzle board as it works

#
#   END: Version 1.6.2
# START: Version 1.7
#

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

#
# v1.7: 39 mil checks/sec; 
#	what the hell? it's got near 100% cpu usage but...
#
#		in million checks/second:
#			 -j1	 -j2	 -j3	 -j4	 -j5	 -j6	 -j7
#	timmpb (4cpu):	69.1*	64.8	52.0	38.6	35.8	36.8	35.1	
#	timvm (1cpu):	71.6	72.7	68.4	67.6	68.2	71.6	72.1*
#	timcent (2cpu):	37.4	37.4	37.4	37.7*	37.7	37.7	37.7
#
#	for j in 1 2 3 4 5 6 7; do echo start j$j;
#	(repeat 3 ./weirdlabsolve -j $j -n 8) | egrep "steps:|Total time"; done
#	Linux: mpstat
#	OS-X:  sysctl -n hw.ncpu
#
# now a running table of million checks/second for each revision...
#
#			perl	1.1   1.2   1.3   1.4   1.5   1.6   1.7
# timmbp (4cpu):	0.014	4.9                    91.5        69.1
# timvm (1cpu):		0.016	4.3   4.3   6.1 *92.x  86.8  25.6  
#
#   END: Version 1.7
# START: Version 1.8
#

- OK, so now 'unwind' the procedure calls so that lexpermute_last6 does
  permutations with 6 steps without any subroutine calls

check processing & steps:

    time ./weirdlabsolve-Darwin1.5 -d -d -d -n 8 | \
    head -5000000 > ~/Downloads/weirdout-1.5-n8_500000.txt

    time ./weirdlabsolve -d -d -d -n 8 -S 1 -j 1 | \
    head -5000000 > ~/Downloads/weirdout-1.8-n8_500000.txt

    diff -uwb weirdout-1.*n8* | cdiff

right that checks out, what about complete coverage of steps when running in
parallel?

    time ./weirdlabsolve-Darwin1.5 -d -d -n 8 2>&1 | \
    head -5000000 | \
    sort > ~/Downloads/weirdout-1.5-steps.txt

    time ./weirdlabsolve -d -d -n 8 -j 2 2>&1 | \
    head -5000000 | \
    sort > ~/Downloads/weirdout-1.8-steps.txt

    diff weirdout-1.*steps* | cut -d, -f 1-3 | sort | uniq -c

that shows that v1.5 has 2487135 extra lines (about 50%) that continue on
solutions starting with move 0; but 1.8 has 2487133 extra lines that work on
solutions starting with move 1; this is probably good enough.

Let us look at the timing again, because I think we are better now.

#
# revision 1.8
#
#		in million checks/second:
#		 -j1	 -j2	 -j3	 -j4	 -j5	 -j6	 -j7	-j8
#timmpb (4cpu):	110.3*	104.7	 83.1	 63.6	 62.5	 64.6	 62.4	62.8
#timmbp-n9	103.2	102.5	 81.2	 64.2	 59.2	 62.1	 60.8	63.4
#timvm (1cpu):	 88.1	 89.2	 87.5 	 87.4	 90.9*	 85.6	 91.5*	89.4
#timcent (2cpu): 72.4	 72.1	 72.4	 72.4	 72.4	 72.4	 72.3	72.3
#gir (8cpu):	146.8*	145.6	141.2	125.0	119.0	109.8	100.9	96.6
#gir-n9		141.8	137.4	139.4	125.9	114.5	107.9	101.6	96.3
#
#...average time now 104.4 mil iterations/sec
#
for j in 1 2 3 4 5 6 7 8 9; do echo start j$j;
(repeat 3 ./weirdlabsolve -j $j -n 8) | egrep "steps:|Total time"; done
#
# comments for revision 1.8
- added lexpermute_seven subroutine that 'unrolls' the recursion from seven
  steps down to one step, to improve speeds.  also added appropriate calling
  of it.
- incrase THREADLEVEL to 8, which means threading steps 7-1 takes 4 sec.
  Should probably change this to 9 or 10.
- create flipout(), which acts like flip() only does not flip the puzzle
  in-place, but instead returns the resulting puzzle value
- improve accuraccy of stopping if stop-after-N-iterations argument is used
- slight tweaks to the debugtiming code, it now works with threading. also
  fixed a math issue in reporting the results.

...now getting fastest results to date, but still no real improvement with
multiple threads.  next try making flipout() inline code via macro

#
#   END: Version 1.8
# START: Version 1.8.1/2
# 
# *sigh* that slows it down, again.
#
    time ./weirdlabsolve-Darwin1.5 -d -d -d -n 8 | \
    head -5000000 > ~/Downloads/weirdout-1.5-n8_500000.txt

    time ./weirdlabsolve -d -d -d -n 8 -S 1 -j 1 | \
    head -5000000 > ~/Downloads/weirdout-1.8-n8_500000.txt

    diff -uwb weirdout-1.*n8* | cdiff

...meanwhile I've implemented a testbed and re-time tested all the code back
to 1.4, and the timing results are NOT the same as I recorded here.  I don't
understand that either.

So I've implemented it with a #define as to whether the macro is on or off.
with FLIPMACRO on,  timvm is faster
with FLIPMACRO off, timcent & timmbp are faster

anyway, onwards...

Version 1.8.2
- turn flip() into a macro, to minimize context switches from within
  lexpermute_seven.  thought this would improve speed, but it only does on one
  host out of three?!  Set its use within lexpermute_seven as a macro
  FLIPMACRO.  Leaving it off for now.
- add a version string and print the version at start. also a version-only argument.
- remove a bunch of debug code from lexpermute_seven
#
#   END: Version 1.8.2
# START: Version 1.9
# 
Right, I dont know why the macro didnt work, but it made me realize that
flip() is too complicated.  Each position being flipped represents an XOR of
3-5 bit positions, or just the XOR of the current puzzle value by a
flip-position-X value.  Now implement a flip lookup table, and make flip a
macro that XORs "puzzle" with flipmap[position].

Automate the code testing in the shell script -- done, and the steps output
shows 0.01% difference.  Looks good.

This improves the speed about 40%... but still no speed improvement from
parallelization?!

    tim-timmbp(weirdlab)% time ./weirdlabsolve -n 8 -j 1
    Total time for sequence of length 8: 34.3 sec, rate 147.9 million/sec

    tim-timmbp(weirdlab)% time ( ./weirdlabsolve -n 8 -j 1 &
			         ./weirdlabsolve -n 8 -j 1 & )
    Total time for sequence of length 8: 36.5 sec, rate 139.2 million/sec
    Total time for sequence of length 8: 36.5 sec, rate 139.2 million/sec
    ...so effectively 139.2*2 == 278.4 mil/sec
    # try 3       ...effectively 346.8 mil/sec
    # try 4       ...effectively 366.1 mil/sec
    # try 5       ...effectively 457.6 mil/sec

Looks like I should just be using fork.

Version 1.9

- make flipout into a macro that uses a static mapping table pos2flip, which
  is initialized with init_pos2flip().  improves speed about 40%

#
#   END: Version 1.9
# START: Version 1.9.1
#
Version 1.9.1 -- make mask in lexpermute_seven into a bit sequence
#
#   END: Version 1.9.1
# START: Version 1.10
# 
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
     5 a d c [index 0 2 1]	13 c a d	2 0 1	21 d b c	3 1 1
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

###
- right, we have implemented BeginIteration and EndIteration -- where to start
  and stop within the entire permutation space, as set on the command line.
  Uses new Factor[] & BeginStep[] globals, makes Steps a global, 
- in the process, realized that a long (4 bytes min) will not hold 20!
  (steps = 20) -- that would take 61 bits, or 8 bytes.  Fortunately a
  "long long" is 64bits in the C99 standard.  Changed all relevant values from
  long to long long.
- Begin/EndIteration require implementing functions to convert between
  permutation number, index into permutation steps, and sequence of moves
  represented.
- change GuessRate & GuessRateHours to real variables settable on the command
  line.  Fixed time estimates.
- argument handling now uses getlongopt to handle --begin and --end arguments.
  re-arranged the listed order of opt handlers to be alphabetical instead of
  kinda random.
- -j 0 will now turn off threading altogether.  Made me realize checkpointing
  and stopping was only in the threaded bit.
- re-arranged globals and definitions a bit.  this file is really too long for
  'production' code.  but this is still hobby code.
- make --split argument to show good places to split the factoring of large
  values of Steps to distribute among multiple hosts
- so much debug cruft in here, the factoring took a lot of getting right.
  i will clean it out in the next minor rev

...in the meantime, keep going with solve12 [use --split now instead of
   calculating these things by 10]

./weirdlabsolve -n 12 -d -d --begin 16314000000000 -S 100 | head -60
...
    remainder               6,       stepsleft  1, BeginStep[ 1] = 6  l
    finishing sequence: 5,8,15,6,13,10,11,12,2,14,9,16
	                a b  c d  e  f  g  h i  j k  l
    ...that is not where it is supposed to start!
    ...or is it:
	    0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
	        i     a d   b    e  f  g  k  h  c  l  j
	    5, 8, 15, 6, 10, 11, 12, 14, 2, 17, 13, 16
	    a  b  c   d   e  f   g   h   i  j   k

Notes from when I tried to use BeginSet[] all the way down to 1 in
lexpermute_seven:

  /* right, this doesn't cut it -- we have to account for any elements
   * 'missing' from the set before index.  e.g.
   * starting set:
   *  set: 0, 1, 2, 3, 4, 7, 9,11,12,13,14,16,17,18,19
   * level 7 uses element 7
   * mask: 0  0  0  0  0  0  0  1  0  0  0  0  0  0  0
   *  set: 0, 1, 2, 3, 4, 7, 9,   12,13,14,16,17,18,19
   * level 6 uses element 7
   * mask: 0  0  0  0  0  0  0  1  1  0  0  0  0  0  0
   *  set: 0, 1, 2, 3, 4, 7, 9,      13,14,16,17,18,19
   * level 5 uses element 8... we can't just go to element 7 and skip
   * those not already in use [result=13], we have to count in 9
   * elements (starting from 0) [result=14]
   *
   * if we look at the 'hamming weight' (number of bits set) under the
   * result we want, we can calculate an offset.  this might be an
   * iterative process tho.
   * there should be a clever binary way to do this but i don't know
   * it
   */

calculating 'h' -- index=8, so first look at bits set under 0-8
	    0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20
	              a d   b    e  f  g               
the hamming weight in 0- 8 is 3, so hoffset=3, try again with 0-11
the hamming weight in 0-11 is 5, so hoffset=5, try again with 0-13
the hamming weight in 0-13 is 6, so hoffset=6, try again with 0-14
the hamming weight in 0-14 is still 6, we have a winner!

    Right... but we ONLY need this if we are trying to get to that exact
    starting sequence.  Which is not strictly necessary as computing all the
    combinations under n=6 is very fast.  Leave out the complex code for
    hamming weights and just stop setting beginning index at BeginStep[7].

    ...
    finishing sequence: 5,8,15,6,10,19,18,17,16,14,13,12
    ? isn\'t this going back
    16131060 iterations in 68.1 sec [16131060 total, 0.24 mil checks/sec]
    Stopping after 16131060 iterations, at iteration # 16314016131060

                                      L  C B  A 9  8  7  6  5 4  3  2  1
                       permute sequence: 5,8,15,6,10,11,12,14,2,17,13,16
finishing sequence      429412240857600: 5,8,15,6,10,11,12,13,2,14,9,16
                                  index: 0,0,0,0,0,0,0,0,0,5,5,6,2


timvm(weirdlab)> ./weirdlabsolve -n 12 --begin 16314000000000 -S 100
Stopping after 16216200 iterations, at iteration # 16314016216200
#
#   END: Version 1.10
# START: Version 1.10.1
# 
remove extra debug code from last round, tidy up some comments
#
#   END: Version 1.10.1
# START: Version 1.10.2
# 
Version 1.10.2

- tidied output in general, added info useful for checking across batch files
  to see that all chunks were run and which host ran which chunk -- and if any
  chunks were successful!
- status updates now include where we are (at least more clearly)
- better calculation of results if only a chunk is processed in one execution
- cleaned up a bit of debug output missed in the last pass
- implement signal handlers to print an update or stop cleanly
- removed notes in .pl from running steps=12

runs taking WAY too long -- is it possible that as I forgot to specify -j0
that the threading loop is starting four identical _11 runs?

make CheckLevel used everywhere
BUT MAKE IT THREAD-SAFE AS CURRENTLY UPDATES A GLOBAL
#
#   END: Version 1.10.2
- start to version 1.10.2.  lexpermute_eleven is just a copy of
  lexpermute_seven, so that when I make changes to convert seven to eleven,
  they will be easier to see in a git diff
- added function definitions and other call bits for eleven
- make lexpermute_seven and lexpermute_eleven use a long long for counting
  internal iterations.  update this anyplace that expected just a long*
  returned from lexpermute*
# START: Version 1.10.3
# 
THEN
create lexpermute_ten or eleven
#
#   END: Version 1.10.3
# START: Version 1.10.4
# 
    Version 1.10.2 on host "M00845"
    steps: 13, solution space:     482718652.4 mil, estimate: ^Wæ5>ÉA^B6.7 we3
    constrained run size:            1270312.2 mil, estimate: ¨ËaFð3.0 3
...it also is doing this:
    Starting top-level element 0
    Steplevel 12 starting element 0.1
    Steplevel 12 starting element 0.1.2
    so an int/long/long long issue on cygwin?

# new for Run 14
--chunk argument instead of begin/end, which understands chunk per level
output includes Begin iteration # and intended end iteration #
--split sizes number of zeros based on number of splits

# longer-term
random output of result puzzle board
test: look for correct recognition of solution.  take a few sample sequences,
    e.g. 16 12 14 1 13 3 9 6 4 2 8 10 17 19
    find what starting puzzle board would make that sequence give a solution,
    then insert that starting-puzzle-board and make sure the code recognizes
    that it does find a solution
test: is order important?  pick a puzzle board result and print all the
    sequences that give that result.  do they all contain the same moves in
    different orders?

## right, well, never mind.  The first chunks of step 14 immediately spit out
results.  In fact, they made it clear that XOR is commutative, so this whole
process should have been a LOT shorter.  i.e. if you are XORing a series of
values, it does not matter what order those values are in.  3 XOR 2 XOR 4
gives the same result as 4 XOR 3 XOR 2.

    Completed permutations 0 - 515908673280, Results 42124320

The first chunk of 515 billion permutations gave 42 million results.  Any
combination of the moves 0, 1, 2, 3, 4, 5, 9, 10, 14, 15, 16, 17, 18, and 19
will solve the puzzle.  Oddly, there is clearly a bug that causes '3' to come
out instead of the other numbers in the success print statement (always at the
end of the statement, so likely a bug in printseq()).

    grep ^success.with Results/n14.top0001 | \
    perl -ne 's/^success with sequence: //;
	      chomp; $l++; my @s=split(","); $three=0;
	      foreach $i (@s) {
		$c{$i}++; $three++ if ($i==3);
		if ($three>1 && !/,3$/) { print "$_\n"}
	      };
	      END {
		print "count, move# (lines $l)\n";
		foreach $k (sort keys %c) {
		    printf("%8d  move %d\n",$c{$k},$k)
		}
	      }' | tee /tmp/movesout

    count, move# (lines 42124320)
    42124320  move 0
    42124320  move 1
    38495520  move 10
    38495520  move 14
    38495520  move 15
    38495520  move 16
    38495520  move 17
    38495520  move 18
    38495520  move 19
    42124320  move 2
    78412320  move 3
    38495520  move 4
    38495520  move 5
    38495520  move 9

This was a very nice stretch of my C coding muscles, exploration of threading
(does not seem v. useful for parallelization on Linux or OSX/FreeBSD!), and
cobbled-together shell scripts & rsync for distributed computing.  On to the
next project.

fix cpu_data so it knows about parallel runs
update speed charts on all hosts, did we slow things down?
review ALL output
convert all ints to unsigned as no need for sign?
https://stackoverflow.com/questions/5248915/execution-time-of-c-program
