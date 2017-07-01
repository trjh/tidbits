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
 */

#ifndef lint
static char *RCSid = "$Header: /Users/tim/proj/src/weirdlab/RCS/weirdsolve.c,v 1.5 2017/07/01 22:39:45 tim Exp tim $";
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <string.h>

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

/* values for tracking progress and regular timing checkpoints */
long permutecount = 0;		 /* how many sequences have we processed? */
#define CHECKINTERVAL 1000000000 /* how often to do a speed check */
long LastCountCheck = 0;	 /* the last permcount value when we did a
				    timecheck on permutations/sec */
long NextCountCheck=CHECKINTERVAL; /* goal for next permcount value when we do
				      a timecheck */

#define SCALE 1000000		/* scale iterations in millions */
#define SCALET "mil"

long StopAfter = 0;		/* if non-zero, stop after this many seqs */

/* only needed when we run testing */
int testseq[5][20] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    {2, 16, 15, 5, 0, 9, 18, 12, 4, 19, 11, 2, 3, 7, 10, 8, 13, 6, 17, 14},
    {17, 4, 5, 3, 16, 14, 18, 19, 1, 7, 9, 13, 12, 6, 0, 11, 10, 15, 2, 8},
    {14, 19, 11, 18, 1, 5, 8, 15, 13, 3, 10, 4, 12, 17, 6, 7, 0, 16, 2, 9},
    {11, 13, 15, 2, 5, 16, 1, 12, 7, 3, 18, 14, 8, 9, 19, 0, 17, 4, 10, 6},
};

clock_t start;			/* start of the entire program */
clock_t lasttimecheck;		/* start of last timecheck */
#define GUESSRATE 80060000	/* last # iterations/sec */

/* structure for executing a run -- needed for threading */
typedef struct lexp_run LEXPERMUTE_ARGS;
struct lexp_run {
    /* i'm a little confused about what Darwin & Linux want in terms of a
     * pointer to an array of ints.  But on Darwin, it doesn't like this:
     * int *solution[]
     * ...but if I just use int *solution, it gives me this:
     * weirdlabsolve.c:336:22: warning: incompatible pointer types assigning
     * to 'int *' from 'int (*)[20]' [-Wincompatible-pointer-types]
     * ...solution seems a bit ugly:
     * downstream.solution = &mysolutionpath[0];
     */
    int *solution;	/* ordered set of values already applied to puzzle,
			   generally of size SIZE, but terminated with a -1.
			   i.e. there may be 20 elements, but if the third
			   value in the array is -1, then only two moves have
			   been made already */
    long puzzle;	/* state of puzzle after solution[] applied */
    int *set;		/* set of possible values to use to solve the rest of
			   the puzzle */
    int setsize;	/* number of elements in set[] */
    int steps;		/* number of steps of solution left to try */
};

/* structure for lexpermute timing */
typedef struct permtimes PERMTIMES;
struct permtimes {
    int steps;                  /* this struct measures lexpermute calls with
				   this number of steps */
    long totalclocks;		/* total clock time for all calls measured */
    int calls;			/* number of times lexpermute called */
};
PERMTIMES lextiming[SIZE];	/* make this a global for up to SIZE steps */

/* predeclare functions */
#define resetpuzzle(puzzle) *puzzle=0x00002940
void printpuzzle(long *puzzle);
void generate(int n, int *sequence, long *puzzle);
void flip(long *puzzle, int position);
void testmode(long *puzzle);
void lexrun(LEXPERMUTE_ARGS *la);
void *lexpermute(void *argstruct);
void printseq(char *, int sequence[], int size, int extra);

/* int errno; */

int main(int argc, char *argv[])
{
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
	start=lasttimecheck=clock();

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
	if (debug>0) {
	    printf("debug level: %d\n", debug);
	}

	/*
	 * now use lexpermute(), which iterates lexically, and is more
	 * efficient with its calls to flip().
	 */
	int j;	/* counter */
	int solutionpath[SIZE];
	solutionpath[0] = -1;

	LEXPERMUTE_ARGS la;
	    la.solution = solutionpath;
	    la.puzzle = puzzle;
	    la.set = moves;
	    la.setsize = SIZE;

	/* initialize lextiming */
	for (j=0; j<SIZE; j++) {
	    lextiming[j].steps = j;
	    lextiming[j].totalclocks = 0;
	    lextiming[j].calls = 0;
	}

	/* if 'steps' was set on the command line, do the run with that many
	 * steps
	 */
	if (steps != SIZE) {
	    la.steps = steps;
	    lexrun(&la);
	}

	/* no steps set -- just run through values of 1 to 10 for steps */
	else {
	    for (steps = 1; steps < 10; steps++) {
		printf("---restart---\n");
		resetpuzzle(&(la.puzzle));
		la.steps = steps;
		lexrun(&la);
	    }
	}

	printf("Elapsed %.1f seconds\n",(double)(clock()-start)/CLOCKS_PER_SEC);
	printf("Timing per level:\n");
	for (j=1; j<SIZE; j++) {
	    if (lextiming[j].calls == 0) { /* we're done */
		break;
	    }
	    double rate = ((double) lextiming[j].totalclocks /CLOCKS_PER_SEC)
			  / lextiming[j].calls;
	    printf("steps % 3d average % 12.1f clocks/run (% 5.1f min/run)\n",
		lextiming[j].steps,
		(double) lextiming[j].totalclocks / (double) lextiming[j].calls,
		rate);
	    #ifdef EXTRADEBUG1
	    printf("\ttotalclocks %ld, calls %d\n",
		lextiming[j].totalclocks, lextiming[j].calls);
	    #endif
	}
}

/* lexrun -- run lexpermute with top-level timing and print output code.  this
 * happens two different times in main(), so tidier to keep it in a subroutine
 */
void lexrun(LEXPERMUTE_ARGS *la)
{
    clock_t startsize, endsize; /* timing variables */
    long solspace = 1;		/* size of solution space */
    long i;			/* counter */
    long *iterations;		/* iterations processed by lexrun() */

    /* calculate the total number of permutations */
    for (i=0; i<la->steps; i++) {
	solspace = solspace * (SIZE-i);
    }

    printf("steps: %d, solution space: %.2f [%ld] "SCALET", "
	   "estimate in hours: %.1f\n", la->steps, (double) solspace/SCALE,
	   solspace,
	   (double) solspace / (GUESSRATE*3600));

    startsize=clock();
    iterations = (long *)lexpermute(la);
    endsize=clock();
    lextiming[(la->steps)-1].totalclocks += endsize - startsize;
    lextiming[(la->steps)-1].calls += 1;

    double elapsed = (double)(endsize - startsize) / CLOCKS_PER_SEC;

    double rate = (double) *iterations / (elapsed * SCALE);
    printf("Total time for sequence of length %d: "
	  "%.1f sec, rate %.1f million/sec\n", la->steps, elapsed, rate);

    if (*iterations != solspace) {
	printf("WARNING! iterations %ld not equal to \n"
	       "         solspace   %ld\n",*iterations,solspace);
	exit(-1);
    }
    
    free(iterations);	/* to balace the malloc() elsewhere */
}

/* given a set of possible moves 'set[]' (array size 'setsize'), and a number
 * of moves 'steps', find all the possible permutations -- and test each one
 * as a solution to 'puzzle'.  if a solution is found, print the solution to
 * standard output, using 'solution[]' as the set of moves already made before
 * this instatiation of the routine.  This routine is recursive.
 */
void *lexpermute(void *argstruct)
{
    /*
    struct lexp_run {
	int *solution[]; ordered set of values already applied to puzzle *
	long puzzle;	 state of puzzle after solution[] applied *
	int *set[];	 set of possible values to use to solve the rest of
			 the puzzle *
	int setsize;	 number of elements in set[] *
	int steps;	 number of steps of solution left to try *
    }
    */
    LEXPERMUTE_ARGS *a = (LEXPERMUTE_ARGS *)argstruct;
    
    long *iterations = malloc(sizeof(long));
    *iterations = 0;
			/* count the number we process internally -- use a
			 * dynamically allocated value for passing back via
			 * phtreads
			 */
    long mypuzzle;	/* local update of puzzle */
    int i, j;		/* counters */
    /* #define EXTRADEBUG1 */

    if (a->steps == 1) {
	/* we have reached the end, check the result(s) & return */
	for (i=0; i<a->setsize; i++) {
	    #ifdef EXTRADEBUG1
	    printf("final step, testing flip of pos %d\n", a->set[i]);
	    #endif
	    if (debug > 1) {
		printseq("finishing sequence: ", a->solution, SIZE, a->set[i]);
	    }
	    
	    *iterations += 1;
	    mypuzzle = a->puzzle;
	    flip(&mypuzzle, a->set[i]);
	    if (mypuzzle == PUZZLECOMPLETE) {
		printseq("\nsuccess with sequence: ", a->solution, SIZE,
						      a->set[i]);
		printpuzzle(&mypuzzle);
	    }

	    /*
	     * we can no longer do spot check of timing & results here, as it
	     * requires updating a global which isn't a good idea if we're in
	     * a 'child thread'.  Do this now at parent thread level.
	     */
	}
	return (void *) iterations;
    }

    /* at this point if steps == StepsToThreadAt, we'll pass all downstream
     * recursive calls to new threads.  But that'll be implemented after we
     * make sure this retooled lexpermute() works without them
     */

    /* if we're here, we have more than one step left to process */
    for (i=0; i<a->setsize; i++) {
	/* at the top level, announce the start of each main branch */
	if (a->solution[0] == -1) {
	    printf("Starting top-level element %d\n", a->set[i]);
	}

	/* update puzzle */
	mypuzzle = a->puzzle;
	flip(&mypuzzle, a->set[i]);

	/* line up arguments for downstream lexpermute call */
	int mysolutionpath[SIZE];
	    /* copy solutionpath and this element into mysolutionpath */
	    for (j = 0; j < SIZE; j++) {
		if (a->solution[j] == -1) {
		    mysolutionpath[j] = a->set[i];
		    mysolutionpath[j+1] = -1;
		    break;
		}
		/* else */
		mysolutionpath[j] = a->solution[j];
	    }
	int myset[a->setsize-1];
	    if (i>0) {
		memcpy(&myset[0],&(a->set[0]),i*sizeof(int));
	    }
	    if (i<(a->setsize-1)) {
		memcpy(&myset[i],&(a->set[i+1]),((a->setsize)-i-1)*sizeof(int));
	    }

	LEXPERMUTE_ARGS downstream;
	downstream.solution = &mysolutionpath[0];
	downstream.puzzle = mypuzzle;
	downstream.set = myset;
	downstream.setsize = (a->setsize)-1;
	downstream.steps = (a->steps)-1;

	#ifdef EXTRADEBUG1
	printf("working element %d, set to pass down: ", a->set[i]);
	for (j=0; j<(a->setsize-1); j++) { printf("%d, ",myset[j]); }
	printf("\n");
	#endif

	clock_t startsize=clock();
	long *tempiterations = (long *)lexpermute(&downstream);
	clock_t endsize=clock();
	*iterations += *tempiterations;
	lextiming[(a->steps)-1].totalclocks += endsize - startsize;
	lextiming[(a->steps)-1].calls += 1;
	free(tempiterations);	/* don't need memory allocated in that call */
    }
    return (void *)iterations;

    /*
    ...and now startpoint can be any array, just process the steps in
    startpoint and call &lexpermute with that many fewer steps
    */
}

void testmode(long *puzzle)
{
    /* explicitly process each of the seqences in @testseq */
    debug=2;
    int i, j;

    for (i=0; i<5; i++) {
	printf("---restart---\n");
	resetpuzzle(puzzle);
	printpuzzle(puzzle);
	printseq("trying ", testseq[i], SIZE, -1);
	for (j=0; j<SIZE; j++) {
	    if (testseq[i][j] == -1) { break; }
	    printf("flip %d:\n", testseq[i][j]);
	    flip(puzzle, testseq[i][j]);
	    printpuzzle(puzzle);
	}
    }

    return;
}

/*
 * subroutine to print the current rate at which we're processing individual
 * sequences -- called from parent thread, and uses global variables:
 * permutecount	-- running total of permutations tried
 * LastCountCheck -- the last permcount value when we did a timecheck on
 *                   permutations/sec
 * NextCountCheck -- goal for next permcount value when we do a timecheck
 * CHECKINTERVAL -- interval between count checks

    * spot checks of timing and results *
    permutecount+=iterations;	-- adding iterations from finished thread
    if (permutecount > NextCountCheck) {
	timecheck();
	LastCountCheck = permutecount;
	NextCountCheck = permutecount + CHECKINTERVAL;
    }

 */
void timecheck()
{
    clock_t t = clock();
    long count_since_last = permutecount - LastCountCheck;
    double rate = ((double) permutecount /
		   (SCALE * (double) (t-start) / CLOCKS_PER_SEC));
    printf("%ld iterations in %0.1f sec "
	   "[%ld total, %.2f "SCALET" checks/sec]\n",
	   count_since_last,
	   (double) (t-lasttimecheck) / CLOCKS_PER_SEC,
	   permutecount, rate);
    lasttimecheck=t;
    if ((StopAfter) && (permutecount >= StopAfter)) {
	printf("Stopping after %ld iterations\n", permutecount);
	exit(0);
    }
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

/* print out a sequence -- and an extra element at the end if non-zero  */
void printseq(char *intro, int sequence[], int size, int extra)
{
    int i;

    /* printf("sequence: "); */
    printf("%s", intro);

    /* don't print a trailing comma at the end */
    printf("%d", sequence[0]);
    for(i = 1; i < size-1; i++) {
	if (sequence[i] == -1) { break; }
	printf(",%d", sequence[i]);
    }
    if (extra > 0) {
	printf(",%d", extra);
    }
    printf("\n");
}
