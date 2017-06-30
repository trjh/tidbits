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
static char *RCSid = "$Header: /home/huntert/proj/tidbits/RCS/weirdsolve.c,v 1.4 2017/06/30 17:04:38 huntert Exp $";
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
#define OUTPUTCHECK 1000000000	/* how often to do a speed check */
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
#define resetpuzzle(puzzle) *puzzle=0x00002940
void printpuzzle(long *puzzle);
void generate(int n, int *sequence, long *puzzle);
void flip(long *puzzle, int position);
void testmode(long *puzzle);
void output(int *sequence, long *puzzle);
int  lexpermute(int *SET,int setsize,long *puzzle,int* solutionpath,int STEPS);

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
	extern char *optarg;
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

	/*
	 * now use lexpermute(), which iterates lexically, and is more
	 * efficient with its calls to flip().
	 */
	int startsize;
	int solutionpath[SIZE];
	solutionpath[0] = -1;
	long guessrate = 80060000;	/* last # iterations/sec */

	startsize=time(NULL);

	long solspace = 1;
	int j;
	long iterations;

	if (steps != SIZE) {
	    for (j=0; j<steps; j++) {
		solspace = solspace * (SIZE-j);
	    }
	    printf("steps: %d, solution space: %.2f [%ld] mil, "
		   "estimate in hours: %ld\n", steps, (double) solspace/1000000,
		   solspace,
		   solspace / (guessrate*3600));
	    iterations = lexpermute(moves, SIZE, &puzzle, solutionpath, steps);
	    int temp=time(NULL);
	    double rate = (double) iterations /
			  (double) ((temp-startsize) * 1000000);
	    printf("Total time for sequence of length %d: "
		  "%d sec, rate %f million/sec\n",
		steps, (temp-startsize), rate);
	    printf("iterations %ld vs. solspace %ld\n",iterations,solspace);
	    exit(0);
	}

	for (steps = 1; steps < 10; steps++) {
	    for (j=0; j<steps; j++) {
		solspace = solspace * (SIZE-j);
	    }
	    printf("steps: %d, solution space: %.2f [%ld] mil, "
		   "estimate in hours: %ld\n", steps, (double) solspace/1000000,
		   solspace,
		   solspace / (guessrate*3600));
	    printf("---restart---\n");
	    resetpuzzle(&puzzle);
	    iterations = lexpermute(moves, SIZE, &puzzle, solutionpath, steps);
	    int temp=time(NULL);
	    double rate = (double) iterations /
			  (double) ((temp-startsize) * 1000000);
	    printf("Total time for sequence of length %d: "
		  "%d sec, rate %f million/sec\n",
		steps, (temp-startsize), rate);
	    printf("iterations %ld vs. solspace %ld\n",iterations,solspace);
	    startsize = temp;
	}

	printf("Elapsed %ld seconds\n",time(NULL)-lasttime);
}

int lexpermute(int *SET,int setsize,long *puzzle,int* solutionpath,int STEPS)
{
    /*
    inputs:
	int SET[] -- set of possible values
	int setsize -- number of elements in SET
	int *puzzle -- link to puzzle space in state we process on
	int *SOLUTIONPATH -- ordered set of values already applied to puzzle
	int STEPS -- number of steps of solution left to try
    */
    long iterations=0;	/* count the number we process internally */
    long mypuzzle;
/* #define EXTRADEBUG1 */

    if (STEPS == 1) {
	/* we have reached the end, check the result(s) & return */
	int i;
	for (i=0; i<setsize; i++) {
	    #ifdef EXTRADEBUG1
	    printf("final step, testing flip of pos %d\n", SET[i]);
	    #endif
	    
	    iterations++;
	    mypuzzle = *puzzle;
	    flip(&mypuzzle, SET[i]);
	    if (mypuzzle == PUZZLECOMPLETE) {
		printf("\nsuccess with seq: ");
		int j=0;
		while(1) {
		    if (solutionpath[j] == -1) {
			break;
		    }
		    printf("%d ", solutionpath[j]);
		}
		printf("%d\n", SET[i]);
		printpuzzle(&mypuzzle);
	    }

	    /* spot checks of timing and results */
	    outputcalls++;
	    if ((outputcalls % OUTPUTCHECK) == 0) {
		time_t t = time(NULL);
		double rate = ((double) outputcalls /
			       (double) (1000000 * (t-start)));
		printf("%d iterations in %ld sec "
		       "[%ld total, %.2f mil checks/sec]\n",
		    OUTPUTCHECK, (t-lasttime), outputcalls, rate);
		lasttime=t;
		if ((StopAfter) && (outputcalls >= StopAfter)) {
		    printf("Stopping after %ld iterations\n", outputcalls);
		    exit(0);
		}
		int tempseq[SIZE];
		for (int j=0; ;j++) {
		    if (solutionpath[j] == -1) {
			tempseq[j] = SET[i];
			tempseq[j+1] = -1;
			break;
		    }
		    tempseq[j] = solutionpath[j];
		}

		long testpuzzle;
		resetpuzzle(&testpuzzle);
		output(&tempseq, &testpuzzle);

		if (debug > 0) {
		    printf (".lexpermute result: \n"); printpuzzle(&mypuzzle);
		    printf (".....output result: \n"); printpuzzle(&testpuzzle);
		}
		if (mypuzzle != testpuzzle) {
		    printf ("RESULT MISMATCH! exit\n");
		    exit(-1);
		}
	    }

	}
	return iterations;
    }

    /* if we're here, we have more than one step left to process */
    int i;
    for (i=0; i<setsize; i++) {
	mypuzzle = *puzzle;
	flip(&mypuzzle, SET[i]);

	int myset[setsize-1];
	int j = 0;
	int mysetpos = 0;
	for (j=0; j<setsize; j++) {
	    if (!(i == j)) {
		myset[mysetpos] = SET[j];
		mysetpos++;
	    }
	}

	#ifdef EXTRADEBUG1
	printf("working element %d, set to pass down: ", SET[i]);
	for (j=0; j<(setsize-1); j++) { printf("%d, ",myset[j]); }
	printf("\n");
	#endif

	int mysolutionpath[SIZE];
	/* copy solutionpath and this element into mysolutionpath */
	for (j = 0; ; j++) {
	    if (solutionpath[j] == -1) {
		mysolutionpath[j] = SET[i];
		mysolutionpath[j+1] = -1;
		break;
	    }
	    /* else */
	    mysolutionpath[j] = solutionpath[j];
	}
	iterations = iterations +
	    lexpermute(myset, setsize-1, &mypuzzle, mysolutionpath, STEPS-1);
    }

    return iterations;

    /*
    ...and now startpoint can be any array, just process the steps in
    startpoint and call &lexpermute with that many fewer steps
    */
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
	    printf("Stopping after %ld iterations\n", outputcalls);
	    exit(0);
	}
    }

    if (debug > 0) {
	/* printf("sequence: "); */
	printf("trying ");
	/* don't print a trailing comma at the end */
	printf("%d", sequence[0]);
	for(i = 1; i < SIZE-1; i++) {
	    if (sequence[s] == -1) { break; }
	    printf(",%d", sequence[i]);
	}
	printf("\n");
    }

    for (s = 0; s < SIZE; s++) {
	if (sequence[s] == -1) { break; }

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
}

void generate(int n, int *sequence, long *puzzle)
{
    int i;

    if (n == 1) {
	/* reset the puzzle space */
	resetpuzzle(puzzle);
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

/* #define EXTRADEBUG2 */
    /* flip position itself */
    *puzzle = *puzzle ^ pos2bit[position];
    #ifdef EXTRADEBUG2
    printf("flip origin %d ",position);
    #endif

    if ((position % WIDTH) < WIDTHLESS1) {
	*puzzle = *puzzle ^ pos2bit[position+1];
	#ifdef EXTRADEBUG2
	printf("flip x+1 %d 0x%x ",position+1,pos2bit[position+1]);
	#endif
    }
    if ((position % WIDTH) > 0) {
	*puzzle = *puzzle ^ pos2bit[position-1];
	#ifdef EXTRADEBUG2
	printf("flip x-1 %d 0x%x ",position-1,pos2bit[position-1]);
	#endif
    }
    if ((position / WIDTH) < HEIGHTLESS1) {
	*puzzle = *puzzle ^ pos2bit[position+WIDTH];
	#ifdef EXTRADEBUG2
	printf("flip y+1 %d 0x%x ",position+WIDTH,pos2bit[position+WIDTH]);
	#endif
    }
    if ((position / WIDTH) > 0) {
	*puzzle = *puzzle ^ pos2bit[position-WIDTH];
	#ifdef EXTRADEBUG2
	printf("flip y-1 %d 0x%x ",position-WIDTH,pos2bit[position-WIDTH]);
	#endif
    }
    #ifdef EXTRADEBUG2
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
