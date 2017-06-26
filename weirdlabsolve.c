/*
 * weirdsolve -- given a puzzle board, find the moves required to turn the
 * whole board from zeros to ones.  Each move causes the place selected to
 * flip (0 to 1 or 1 to 0), but also flips those directly adjacent.  Defined
 * by "Weird Lab" in Gumballs & Dungeons
 *
 * Initial position/configuration of board on third floor:
 *
 * 0 0 0 0 0
 * 0 1 0 1 0
 * 0 1 0 1 0
 * 0 0 0 0 0
 *
 * (Some legacy elements remain in code from puzzle on 2nd floor which was a
 * 4x4 matrix)
 */

#ifndef lint
static char *RCSid = "$Header: /home/huntert/proj/tidbits/RCS/weirdsolve.c,v 1.1 2017/06/26 23:29:23 tim Exp $";
#endif

#include <stdio.h>
#include <errno.h>
#include <time.h>

/*
 * definitions
 *
 */

#define WIDTH	5	/* width of puzzle space */
#define HEIGHT	4	/* height of puzzle space */
#define SIZE	20	/* elements in puzzle space */

/*
 * globals
 */

int debug = 0;
int puzzle[WIDTH][HEIGHT];				/* puzzle space */
int sequence[SIZE] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,	/*seq of moves*/
		      10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

int successlength = 99;		/* once we've found a successful sequence,
				   keep going and print again if we find a
				   shorter one. */
long outputcalls = 0;		/* how many sequences have we processed? */
#define OUTPUTCHECK 10000000	/* how often to do a speed check */

time_t start;			/* for examining how long things take */
time_t lasttime;		/* for examining how long things take */

/* predeclare functions */
void resetpuzzle(int puzzle[HEIGHT][WIDTH]);
void printpuzzle(int puzzle[HEIGHT][WIDTH]);
void generate(int n, int *sequence, int puzzle[HEIGHT][WIDTH]);
void flip(int puzzle[HEIGHT][WIDTH], int position);

/* int sys_nerr; */
/* char *sys_errlist[]; */
int errno;

int main()
{

	time_t t;
	int puzzle[HEIGHT][WIDTH];

	/* initialize puzzle */
	resetpuzzle(puzzle);

	start=lasttime=time(NULL);
	
	/* run heaps algorithm, which calls output() to evaluate each
	 * individual sequence combination over puzzle to see if it works
	 */
	generate(SIZE, sequence, puzzle);

	fprintf(stderr, "hello, world!\n");

	printpuzzle(puzzle);

	/* testing...
	printf("Flip position 0\n");
	flip(puzzle, 0);
	printpuzzle(puzzle);

	printf("Flip position 6\n");
	flip(puzzle, 6);
	printpuzzle(puzzle);
	*/

	printf("Elapsed %ld seconds\n",time(NULL)-lasttime);
}

void resetpuzzle(int puzzle[HEIGHT][WIDTH])
{
    /*
    puzzle = {
	{0, 0, 0, 0, 0},
	{0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0},
	{0, 0, 0, 0, 0}
    };
    */

    int x, y;
    for(y = 0; y < HEIGHT; y++) {
	for(x = 0; x < WIDTH; x++) {
	    puzzle[y][x] = 0;
	}
    }
    puzzle[1][1]=1;
    puzzle[1][3]=1;
    puzzle[2][1]=1;
    puzzle[2][3]=1;

    return;
}

void output(int *sequence, int puzzle[HEIGHT][WIDTH])
{
    int i, s, x, y;
    outputcalls++;
    if ((outputcalls % OUTPUTCHECK) == 0) {
	time_t t = time(NULL);
	double rate = ((double) outputcalls / (double) (1000000 * (t-start)));
	printf("%d iterations in %ld sec [%ld total, %.2f mil checks/sec]\n",
	    OUTPUTCHECK, (t-lasttime), outputcalls, rate);
	lasttime=t;
    }

    if (debug > 0) {
	printf("sequence: ");
	for(i = 0; i < SIZE; i++) {
	    printf("%d ", sequence[i]);
	}
	printf("\n");
    }

    for (s = 0; s < SIZE; s++) {
	flip(puzzle, sequence[s]);
	if (debug > 1) {
	    printf("flip %d:\n", sequence[s]);
	    printpuzzle(puzzle);
	}
	/* test for completion */
	int success=1;
	for(y = 0; y < HEIGHT; y++) {
	    for(x = 0; x < WIDTH; x++) {
		if (puzzle[y][x] == 0) { success=0; break; }
	    }
	    if (success == 0) { break; }
	}
	if (success == 1) {
	    if ((s+1) < successlength) {
		successlength=s+1;
		printf("\nsuccess in %d with seq: ",successlength);
		for(i = 0; i < s+1; i++) {
		    printf("%d ", sequence[i]);
		}
		printf("\n");
	    }
	    break;
	}
    }

    /* reset the puzzle space */
    resetpuzzle(puzzle);
}

void generate(int n, int *sequence, int puzzle[HEIGHT][WIDTH])
{
    int i;

    if (n == 1) {
	output(sequence, puzzle);
	return;
    }
    for (i = 0; i < (n-1); i++) {
	generate(n - 1, sequence, puzzle);
	if (n % 2) {
	    /* $n is odd, swap(A[0], A[n-1]) */
	    register int a = sequence[n-1];
	    register int b = sequence[0];
	    sequence[0] = a;
	    sequence[n-1] = b;
	} else {
	    /* $n is even, swap(A[i], A[n-1]) */
	    register int a = sequence[n-1];
	    register int b = sequence[i];
	    sequence[i] = a;
	    sequence[n-1] = b;
	}
    }
    generate(n-1, sequence, puzzle);
}

void flip(int puzzle[HEIGHT][WIDTH], int position)
{
    /*
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
    */
    int x = position % WIDTH;
    int y = position / WIDTH;

    /* flip position itself */
    puzzle[y][x] = 0 + !(puzzle[y][x]);
    
    /* and all adjacent */
    if (y > 0) {
	puzzle[y-1][x] = 0 + !(puzzle[y-1][x]);
    }
    if (x > 0) {
	puzzle[y][x-1] = 0 + !(puzzle[y][x-1]);
    }
    if (y < (HEIGHT-1)) {
	puzzle[y+1][x] = 0 + !(puzzle[y+1][x]);
    }
    if (x < (WIDTH-1)) {
	puzzle[y][x+1] = 0 + !(puzzle[y][x+1]);
    }
}
/* print out the puzzle matrix */
void printpuzzle(int puzzle[HEIGHT][WIDTH])
{
    int x, y;

    for(y = 0; y < HEIGHT; y++) {
	for(x = 0; x < WIDTH; x++) {
	    printf("%d ", puzzle[y][x]);
	}
	printf("\n");
    } 
}

/*

#!/usr/bin/perl -w

use strict;
$|++;

# solve the weird lab 3rd floor

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
my $ltime = time();

if ($ARGV[0] =~ /^-i/i) {
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
## heap's algorithm will re-arrange @seq in each possible combination, calling
## &output on each one.
sub output {
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
*/
