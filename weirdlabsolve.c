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
static char *RCSid = "$Header: /home/huntert/proj/tidbits/RCS/weirdsolve.c,v 1.3 2017/06/29 17:15:08 huntert Exp $";
#endif

#include <unistd.h>
#include <stdlib.h>
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

#define WIDTHLESS1 (WIDTH-1)	/* used by flip() */
#define HEIGHTLESS1 (HEIGHT-1)

/*
 * globals
 */

int debug = 0;

/* possible moves */
static int moves[SIZE] = { 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
			  10, 11, 12, 13, 14, 15, 16, 17, 18, 19};

/* moves mapped to bit placements */
static long pos2bit[SIZE] =	{0x0001, 0x0002, 0x0004, 0x0008, 0x0010,
				 0x0020, 0x0040, 0x0080, 0x0100, 0x0200,
				 0x0400, 0x0800, 0x1000, 0x2000, 0x4000,
				 0x8000, 0x10000, 0x20000, 0x40000, 0x80000};

/* a successfully solved puzzle */
#define PUZZLECOMPLETE 0x000FFFFF

long outputcalls = 0;		/* how many sequences have we processed? */
#define OUTPUTCHECK 10000000	/* how often to do a speed check */
long StopAfter = 0;		/* if non-zero, stop after this many seqs */

/* only needed when we run testing */
int testseq[5][20] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    {2, 16, 15, 5, 0, 9, 18, 12, 4, 19, 11, 2, 3, 7, 10, 8, 13, 6, 17, 14},
    {17, 4, 5, 3, 16, 14, 18, 19, 1, 7, 9, 13, 12, 6, 0, 11, 10, 15, 2, 8},
    {14, 19, 11, 18, 1, 5, 8, 15, 13, 3, 10, 4, 12, 17, 6, 7, 0, 16, 2, 9},
    {11, 13, 15, 2, 5, 16, 1, 12, 7, 3, 18, 14, 8, 9, 19, 0, 17, 4, 10, 6},
};

time_t start;			/* for examining how long things take */
time_t lasttime;		/* for examining how long things take */

/* predeclare functions */
/* void resetpuzzle(long *puzzle); */
#define resetpuzzle(puzzle) *puzzle=0x2940
void printpuzzle(long *puzzle);
void generate(int n, int *sequence, long *puzzle);
void flip(long *puzzle, int position);
void testmode(long *puzzle);
void output(int *sequence, long *puzzle);

/* int sys_nerr; */
/* char *sys_errlist[]; */
int errno;

int main(int argc, char *argv[])
{
	time_t t;
	long puzzle;	/* puzzle space,
			   positions represented by individual bits
			    0  1  2  3  4 == 0x0001 0x0002 0x0004 0x0008 0x0010
			    5  6  7  8  9    0x0020 0x0040 0x0080 0x0100 0x0200
			   10 11 12 13 14    0x0400 0x0800 0x1000 0x2000 0x4000
			   15 16 17 18 19    0x8000 0x10000 0x20000 0x40000 ...
			*/
	int steps = SIZE;

	/* initialize puzzle and timers */
	resetpuzzle(&puzzle);
	start=lasttime=time(NULL);

	/* process options */
	int flags, opt;
	while ((opt = getopt(argc, argv, "dtn:S:")) != -1) {
	    switch(opt) {
	    case 'd':
		debug++;
		break;
	    case 't':
		testmode(&puzzle);
		exit(0);
	    case 'n':
		steps = atoi(optarg);
		break;
	    case 'S':
		StopAfter = atoi(optarg);
		break;
	    default: /* '?' */
		fprintf(stderr, "Usage: %s [-d] [-t] [-n steps]\n"
				"\t-d	: increase debug value\n"
				"\t-t	: test mode -- just show details of "
					 "processing 5 sequences of 20\n"
				"\t-n N	: examine all solutions with N steps\n"
				"\t-S M	: stop after M iterations\n",
				argv[0]);
		exit(EXIT_FAILURE);
	    }
	}
			
	/*
	 * generate() uses heaps algorithm, which calls output() to evaluate
	 * each individual sequence combination over puzzle to see if it works
	 * ...and between those two it's going to take 25 trillion years to
	 * run
	 * generate(SIZE, moves, &puzzle);
	 */
	generate(SIZE, moves, &puzzle);

	int sizes, startsize;
	startsize=time(NULL);
	for (sizes = 1; sizes < 6; sizes++) {
	    generate(sizes, moves, &puzzle);
	    /* reset sequence */
	    int temp;
	    for (temp = 0; temp < SIZE; temp++) {
		moves[temp] = temp;
	    }
	    temp=time(NULL);
	    printf("Total time for sequence of length %d: %d sec\n",
		sizes, (temp-startsize));
	    startsize = temp;
	}

	printf("Elapsed %ld seconds\n",time(NULL)-lasttime);
}

void testmode(long *puzzle)
{
    /* explicitly process each of the seqences in @testseq */
    debug=2;
    int i;

    for (i=0; i<5; i++) {
	printf("---restart---\n");
	resetpuzzle(puzzle);
	printpuzzle(puzzle);
	output(testseq[i], puzzle);
    }

    return;
}


/* now we can just make this a macro! */

void resetpuzzle_unused(long *puzzle)
{
    /*
    puzzle = {
	{0, 0, 0, 0, 0},
	{0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0},
	{0, 0, 0, 0, 0}
    };
    ...in the old way
    */

    *puzzle=0;	/* clear the puzzle space */

    /* set positions 6, 8, 11, and 13 */
    *puzzle = *puzzle | pos2bit[6] | pos2bit[8] | pos2bit[11] | pos2bit[13];

    return;
}

void output(int *sequence, long *puzzle)
{
    int i, s, x, y;
    outputcalls++;
    if ((outputcalls % OUTPUTCHECK) == 0) {
	time_t t = time(NULL);
	double rate = ((double) outputcalls / (double) (1000000 * (t-start)));
	printf("%d iterations in %ld sec [%ld total, %.2f mil checks/sec]\n",
	    OUTPUTCHECK, (t-lasttime), outputcalls, rate);
	lasttime=t;
	if ((StopAfter) && (outputcalls >= StopAfter)) {
	    printf("Stopping after %d iterations\n", outputcalls);
	    exit(0);
	}
    }

    if (debug > 0) {
	/* printf("sequence: "); */
	printf("trying ");
	for(i = 0; i < SIZE-1; i++) {
	    printf("%d,", sequence[i]);
	}
	/* don't print a trailing comma at the end */
	printf("%d\n", sequence[SIZE-1]);
    }

    for (s = 0; s < SIZE; s++) {
	flip(puzzle, sequence[s]);
	if (debug > 1) {
	    printf("flip %d:\n", sequence[s]);
	    printpuzzle(puzzle);
	}
	/* test for completion -- pretty easy now! */
	if (*puzzle == PUZZLECOMPLETE) {
	    printf("\nsuccess in %d with seq: ",(s+1));
	    for(i = 0; i < s+1; i++) {
		printf("%d ", sequence[i]);
	    }
	    printf("\n");
	    break;
	}
    }

    /* reset the puzzle space */
    resetpuzzle(puzzle);
}

void generate(int n, int *sequence, long *puzzle)
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

void flip(long *puzzle, int position)
{
    /*
    # flip monster in position $x, which also flips those in all adjacent
    # positions
    # pos map:
    # 0 1 2 3
    # 4 5 6 7
    # 8 9 A B
    # C D E F
    # ...but now
    # 0 1 2 3 4
    # 5 6 7 8 9
    # A B C D E
    # F G H I J
    */
    /*
    puzzle[HEIGHT*WIDTH]
    x+1, P+1 -- no for 4,9,14,19 P%WIDTH=WIDTH-1
    x-1, P-1 -- no for 0,5,10,15 P%WIDTH=0
    y+1, P+5 -- no for 15-19 P/WIDTH=HEIGHT-1
    y-1, P-5 -- no for 0-4 P/WIDTH=0

    a flip is an XOR -- ^ in C parlance
    */

/* #define EXTRADEBUG */
    /* flip position itself */
    *puzzle = *puzzle ^ pos2bit[position];
    #ifdef EXTRADEBUG
    printf("flip origin %d ",position);
    #endif

    if ((position % WIDTH) < WIDTHLESS1) {
	*puzzle = *puzzle ^ pos2bit[position+1];
	#ifdef EXTRADEBUG
	printf("flip x+1 %d 0x%x ",position+1,pos2bit[position+1]);
	#endif
    }
    if ((position % WIDTH) > 0) {
	*puzzle = *puzzle ^ pos2bit[position-1];
	#ifdef EXTRADEBUG
	printf("flip x-1 %d 0x%x ",position-1,pos2bit[position-1]);
	#endif
    }
    if ((position / WIDTH) < HEIGHTLESS1) {
	*puzzle = *puzzle ^ pos2bit[position+WIDTH];
	#ifdef EXTRADEBUG
	printf("flip y+1 %d 0x%x ",position+WIDTH,pos2bit[position+WIDTH]);
	#endif
    }
    if ((position / WIDTH) > 0) {
	*puzzle = *puzzle ^ pos2bit[position-WIDTH];
	#ifdef EXTRADEBUG
	printf("flip y-1 %d 0x%x ",position-WIDTH,pos2bit[position-WIDTH]);
	#endif
    }
    #ifdef EXTRADEBUG
    printf("\n");
    #endif

    return;
}
/* print out the puzzle matrix */
void printpuzzle(long *puzzle)
{
    int x, y, i;

    for(y = 0; y < HEIGHT; y++) {
	for(x = 0; x < WIDTH; x++) {
	    printf("%d ", (*puzzle & pos2bit[x + WIDTH*y] ? 1 : 0));
	}
	printf("\n");
    } 
}
