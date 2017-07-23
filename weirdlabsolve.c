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
static char *Version = "1.9.2";

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include "binary.h"
#include <signal.h>

/*
 * definitions
 *
 */

#define WIDTH	5	/* width of puzzle space */
#define HEIGHT	4	/* height of puzzle space */
#define SIZE	20	/* elements in puzzle space */

#define WIDTHLESS1 (WIDTH-1)	/* used by flip() */
#define HEIGHTLESS1 (HEIGHT-1)

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

/*
 * globals
 */

int debug = 0;

/* #define EXTRADEBUG1 -- print more checkpoint details */
/* #define EXTRADEBUG2 -- print debug steps from flip() */
/* #define DEBUGTIMING -- track timings of execution at each level of
			  recursion */

/* we need the overall number of steps being processed to be a global when
 * calculating permutation numbers down at the level where the number of steps
 * is only defined as the number of steps remaining to permute
 */
int Steps = SIZE;

    /* factor to use to go from iteration number to position in moves array.
       index is number of steps remaining, so
       factor[STEPS]   = SIZE-1*...*SIZE-STEPS+1
       factor[STEPS-1] = SIZE-2*...*SIZE-STEPS+1
       ...
    */
long long Factor[SIZE+1]={ 0 };	/* key starts with 1.  only needs to be size
				   STEPS, but we don't know how many steps
				   we're trying until later. Defined in
				   init_factor() */

/* flips mapped to bit placements -- dynamically generated for now */
long pos2flip[SIZE];

/* values for tracking progress and regular timing checkpoints */
long long permutecount = 0;
				 /* how many sequences have we processed? */

/* how often to do a speed check: now make this
   our standard estimate x 6 x 60 == 6 minutes
*/
#define CHECKINTERVAL   42984000000

long long LastCountCheck = 0;	/* the last permcount value when we did a
				    timecheck on permutations/sec */
long long NextCountCheck=CHECKINTERVAL;
				/* goal for next permcount value when we do
				      a timecheck */
int CheckLevel = 8;		/* Level to checkpoint at, really only used if
				   Threading is disabled */

long long BeginIteration = 0;	/* iteration to start at */
long long EndIteration = 0;	/* iteration to end at,
				   if we're not doing full range */
int BeginStep[SIZE+1] = {0};	/* each step will start at BeginStep[step],
				   non-zero if BeginIteration is non-zero.
				   See init_begin() for more details */

/* value to determine where to thread, and which threads are complete */
int ThreadLevel = 8;		/* do threading if steps == ThreadLevel */
int Threads = 4;	
pthread_t tid_main;		/* thread id of main */
#define NOTHREADS 1000		/* if ThreadLevel == NOTHREADS, we are not
				   threading */

#define SCALE 1000000		/* scale iterations in millions */
#define SCALET "mil"

long long StopAfter = 0;	/* if non-zero, stop after this many seqs */

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

float	GuessRate = 119.4*1000000; /* second slowest average
				   million-iterations-per-second
				   as measured amongst my hosts.
				   Can be changed on command line. */
    
int Results = 0;		/* We don't expect this to be set much */
/* structure for executing a run -- needed for threading */
/* now store the solution & set arrays directly in the structure, to make
 * things a little cleaner, code-wise
 */
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
    int solution[SIZE];	/* ordered set of values already applied to puzzle,
			   generally of size SIZE, but terminated with a -1.
			   i.e. there may be 20 elements, but if the third
			   value in the array is -1, then only two moves have
			   been made already */
    long puzzle;	/* state of puzzle after solution[] applied */
    int set[SIZE];	/* set of possible values to use to solve the rest of
			   the puzzle */
    int setsize;	/* number of elements in set[] */
    int steps;		/* number of steps of solution left to try */
};
#define LEXP_SETSIZE SIZE*sizeof(int)
			/* macro so that we don't do this as a computation
			 * later when the value is essentially static
			 */

#ifdef DEBUGTIMING
/* structure for lexpermute timing */
typedef struct permtimes PERMTIMES;
struct permtimes {
    int steps;                  /* this struct measures lexpermute calls with
				   this number of steps */
    long totalclocks;		/* total clock time for all calls measured */
    int calls;			/* number of times lexpermute called */
};
PERMTIMES lextiming[SIZE];	/* make this a global for up to SIZE steps */
#endif

/* predeclare functions */
#define resetpuzzle(puzzle) *puzzle=0x00002940
void printpuzzle(long *puzzle);
void generate(int n, int *sequence, long *puzzle);
void flip(long *puzzle, int position);
long flipout_function(long puzzle, int position);
void testmode(long *puzzle);
void lexrun(LEXPERMUTE_ARGS *la);
void *lexpermute(void *argstruct);
void *lexpermute_seven(void *argstruct);
void printseq(FILE *stream, char *, int sequence[], int size, int extra);
void revprintseq(FILE *stream, char *, int sequence[], int size, int extra);
void timecheck(int *solution, int laststep);
void print_lexargs(char *intro, LEXPERMUTE_ARGS *a);
void init_pos2flip();
void init_factor(int steps);
void show_split();
char *sec2string(long long seconds);
void signal_stop(int signal);
void signal_info(int signal);

/* with the implementation of pos2flip, flipout can be a macro
   ...we kept the function version for now, renamed with a _function */
#define flipout(puzzle, position) ( puzzle ^ pos2flip[position] )

/* macro version of flip -- works on a long directly, not the pointer */
#define flipm(puzzle, pos) { \
    puzzle = puzzle ^ pos2bit[pos]; \
    if ((pos % WIDTH) < WIDTHLESS1)  { puzzle= puzzle ^ pos2bit[pos+1]; }; \
    if ((pos % WIDTH) > 0)           { puzzle= puzzle ^ pos2bit[pos-1]; }; \
    if ((pos / WIDTH) < HEIGHTLESS1) { puzzle= puzzle ^ pos2bit[pos+WIDTH];} \
    if ((pos / WIDTH) > 0)	     { puzzle= puzzle ^ pos2bit[pos-WIDTH];} \
    }

/* the XORs in flip can be combined into a fast lookup table.  we could hard
 * code it but we'll just generate it using flip() for clarity */

/* macros to convert between:
 * permutation number -- 0 to (total number of permutations)
 * permutation index  -- index values into array of available moves at each
 * 			 level.
 *			 ...this reads "backwards" as the first choice/move in
 *			 the permutation is index[steps+1] and the last is
 *			 index[1]
 * sequence           -- the actual sequence of moves corresponding to the
 * 			 above... returned in order of making them (i.e.
 * 			 opposite to the order in the index)
 *
 * e.g. moves [a, b, c, d], steps=3, total permutations: 24
 *         permutation	#index	sequence
 *                   0	0,0,0	a,b,c
 *                   1	0,0,1	a,b,d
 *		    11  1,2,1	b,d,c
 *		    23  3,2,1	d,c,b
 *
 * These macros assume we are working on the GLOBAL values of moves[], SIZE,
 * and Factor[]
 *
 */

/* assumes:
 * 	permnum: permutation number (int)
 *	steps:	 number of steps we're using (size of sequence)
 * 	index:	 int array of size [steps+1], already defined
 *	globals as above
 */
#define permute2index(permnum, steps, index) {	\
    long remainder = permnum;		\
    int pi_counter; \
    for (pi_counter=0; pi_counter < (steps-1); pi_counter++) { \
	int stepsleft = steps-pi_counter; \
	if (debug>1) { printf("remainder %15ld, i %2d, ", \
			      remainder, pi_counter); } \
	index[stepsleft] = remainder / Factor[stepsleft-1]; \
	remainder        = remainder % Factor[stepsleft-1]; \
	if (debug>1) { printf("stepsleft %2d, index[%2d] = %d\n", \
			      stepsleft, stepsleft, index[stepsleft]); } \
    } \
    index[1] = remainder; \
    if (debug>1) { printf("remainder %15ld,       stepsleft %2d, " \
			  "index[%2d] = %d\n", remainder, 1, 1, index[1]); } \
}

/* assumes:
 * 	index:	 int array of size [steps+1], already defined
 *		 where index[steps+1]...index[1] is the sequence order
 *	steps:	 number of steps we're using (size of sequence)
 *	sequence:int array of size [steps], already defined
 *		 sequence runs in opposite order to index
 *	globals as above
 */
#define index2sequence(index, steps, sequence) { \
    long mask = 0; \
    int index_i, moves_i; \
    for (index_i=steps; index_i > 0; index_i--) { \
	int level_index = 0; \
	for (moves_i=0; moves_i < SIZE; moves_i++) { \
	    int my_move=(1 << moves_i); \
	    if (mask & my_move) { continue; } \
	    if (level_index == index[index_i]) { \
		sequence[steps-index_i] = moves[moves_i]; \
		mask |= my_move; \
		break; \
	    } \
	    level_index++; \
	    if (level_index>(steps+1)) { \
		fprintf(stderr,"math issue index_i %d, level_index %d, " \
			       "moves_i %d\n", index_i, level_index, moves_i); \
	    } \
	} \
    } \
}

/* assumes:
 * 	index:	 int array of size [steps+1], already defined
 *	steps:	 number of steps we're using (size of sequence)
 *	result:  long long where we place result, defined externally
 * 	permnum: long long where we place permutation number, defined externally
 *	globals as above
 */
#define index2permute(index, steps, permnum) { \
    int index_i; \
    permnum = index[1]; \
    for (index_i=2; index_i <= steps; index_i++) { \
	permnum += index[index_i]*Factor[index_i-1]; \
    } \
}

/* assumes:
 *	sequence:int array of size [steps], already defined
 *		 sequence runs in opposite order to index
 *	steps:	 number of steps we're using (size of sequence)
 * 	index:	 int array of size [steps+1], already defined
 *	globals as above
 */
#define sequence2index(sequence, steps, index) { \
    long mask = 0; \
    int sequence_i, moves_i; \
    index[0] = 0; \
    for (sequence_i=0; sequence_i < steps; sequence_i++) { \
	int index_v = 0; \
	for (moves_i=0; moves_i < SIZE; moves_i++) { \
	    int my_move=(1 << moves_i); \
	    if (mask & my_move) { continue; } \
	    if (sequence[sequence_i] == moves[moves_i]) { \
		index[steps-sequence_i] = index_v; \
		mask |= my_move; \
		break; \
	    } \
	    index_v++; \
	} \
    } \
}
    
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
	tid_main = pthread_self();

	/* initialize flip map, puzzle, and timers */
	init_pos2flip();
	resetpuzzle(&puzzle);
	start=lasttimecheck=clock();

	/* process options */
	int ShowSplit = 0;
	int flags, opt;
	extern char *optarg;
	while (1) {
	    static struct option long_options[] =
	    {
		{"begin",	required_argument, 0, 'b'},
		{"end",		required_argument, 0, 'f'},
		{"split",	no_argument,	   0, 's'},
		{0, 0, 0, 0}
	    };
	    int option_index = 0;
	    
	    opt = getopt_long(argc, argv, "de:j:n:S:tv",
			      long_options, &option_index);

	    if (opt == -1) break;

	    switch(opt) {
	    case 'b':
		/* --begin option */
		BeginIteration = atoll(optarg);
		break;
	    case 'd':
		debug++;
		break;
	    case 'e':
		GuessRate = atof(optarg) * 1000000;
		break;
	    case 'f':
		/* --end option */
		EndIteration = atoll(optarg);
		break;
	    case 'j':
		Threads = atoi(optarg);
		if (Threads == 0) {
		    ThreadLevel = NOTHREADS;
		    Threads = 1; 	/* minimally dimension arrays used in
					   lexpermute() with size "Threads" */
		}
		break;
	    case 'n':
		Steps = atoi(optarg);
		break;
	    case 'S':
		StopAfter = atoll(optarg);
		break;
	    case 's':
		/* --split option -- we just need to run show_split and exit,
		 * but we should let the command-line processing finish */
		ShowSplit = 1;
		break;
	    case 't':
		testmode(&puzzle);
		exit(0);
	    case 'v':
		printf("Version %s\n", Version);
		exit(0);
	    default: /* '?' */
		fprintf(stderr, "Usage: %s [-d] [-t] [-n steps]\n"
			"\t-d	: increase debug value\n"
			"\t-e N : estimated speed in mil-iterations/sec\n"
			"\t-j T : number of threads to use (0: no threading)\n"
			"\t-n N	: examine all solutions with N steps\n"
			"\t-S M	: stop after M iterations\n"
			"\t-t	: test mode -- just show details of "
				 "processing 5 sequences of 20\n"
			"\t-v	: show version number and exit\n"
			"\t--begin (N|V,..)	: specify the iteration to "
			    "start at as a\n"
			    "\t\t\t\t  whole value or series of indexes"
			"\t--end (N|V,..)	: specify iteration to end at "
			    "as a\n"
			    "\t\t\t\t  whole value or series of indexes"
			,
			argv[0]);
		exit(EXIT_FAILURE);
	    }
	}

	{
	    char h[80];
	    gethostname(h, 80);
	    char *dot = strchr(h, (int) '.');
	    /* don't print anything after the dot in hostname*/
	    if (dot) { *dot = '\0'; }
	    printf("Version %s on host \"%s\"\n", Version, h);
	}
	if (debug>0) {
	    printf("debug level: %d\n", debug);
	}
	/* initialize Factor[] array, and BeginStep[] if needed */
	init_factor(Steps);
	if (BeginIteration>0) {
	    permute2index(BeginIteration, Steps, BeginStep);
	    if (debug > 1) {
		/*
		 * from initial debug of these routines
		 */
		printf(          "Permute        #: %lld\n", BeginIteration);
		printseq(stdout, "permute    index: ", BeginStep, Steps+1, -1);
		int BeginSeq[Steps];
		index2sequence(BeginStep, Steps, BeginSeq);
		printseq(stdout, "permute sequence: ", BeginSeq, Steps, -1);
		long long permnum = 0;
		index2permute(BeginStep, Steps, permnum);
		printf(          "Perm# from index: %lld\n", permnum);
		int index[Steps+1];
		sequence2index(BeginSeq, Steps, index);
		printseq(stdout, "permIdx from seq: ", index, Steps+1, -1);
	    }
	}
	/* if we're just showing splits, do that and exit */
	if (ShowSplit == 1) {
	    show_split(Steps);
	    exit(0);
	}

	/* initialize signal handlers */
	signal(SIGHUP,  signal_stop);
	signal(SIGINT,  signal_stop);
	signal(SIGINFO, signal_info);

	/* figure out when we stop -- rationalize StopAfter and EndIteration.
	 * We'll continue to use StopAfter as the stopping trigger
	 */
	if (StopAfter && EndIteration) {
	    /* prefer the shorter of the two */
	    if (StopAfter > (EndIteration - BeginIteration)) {
		fprintf(stderr, "Warning: EndIteration %lld specifies fewer "
				"iterations to run than\n\t StopAfter (%lld). "
				"Will use EndIteration.\n",
		    EndIteration, StopAfter);
		StopAfter = EndIteration - BeginIteration;
	    }
	    else if (StopAfter < (EndIteration - BeginIteration)) {
		fprintf(stderr, "Warning: StopAfter %lld specifies fewer "
				"iterations to run than\n\t EndIteration (%lld)"
				". Will use StopAfter.\n",
		    StopAfter, EndIteration);
	    }
	}
	else if (EndIteration) {
	    StopAfter = EndIteration - BeginIteration;
	}
	/* now if StopAfter exists, set us up to stop in the right place */
	if ((StopAfter) && (NextCountCheck > StopAfter)) {
	    NextCountCheck = StopAfter;
	}

	/* change stdout to unbuffered, so we see log files updated in
	 * realtime when using pipes to tee. $|++ in perl... */
	setvbuf(stdout, NULL, _IONBF, 0);

	/*
	 * now use lexpermute(), which iterates lexically, and is more
	 * efficient with its calls to flip().
	 */
	int j;	/* counter */

	LEXPERMUTE_ARGS la;
	    la.solution[0] = -1;
	    la.puzzle = puzzle;
	    memcpy(la.set,moves,LEXP_SETSIZE);
	    la.setsize = SIZE;

	#ifdef DEBUGTIMING
	/* initialize lextiming */
	for (j=0; j<SIZE; j++) {
	    lextiming[j].steps = j;
	    lextiming[j].totalclocks = 0;
	    lextiming[j].calls = 0;
	}
	#endif

	/*
	 * if 'steps' was set on the command line, do the run with that many
	 * steps
	 */
	if (Steps != 0) {
	    la.steps = Steps;
	    lexrun(&la);
	}

	/* no steps set -- just run through values of 1 to 10 for steps */
	else {
	    for (Steps = 1; Steps < 10; Steps++) {
		printf("---restart---\n");
		resetpuzzle(&(la.puzzle));
		la.steps = Steps;
		lexrun(&la);
	    }
	}

	printf("Elapsed %.1f seconds\n",(double)(clock()-start)/CLOCKS_PER_SEC);

	#ifdef DEBUGTIMING
	printf("Timing per level: (%d clocks/sec)\n", CLOCKS_PER_SEC);
	for (j=1; j<SIZE; j++) {
	    if (lextiming[j].calls == 0) { /* we're done */
		continue;
	    }
	    double rate = ((double) lextiming[j].totalclocks /CLOCKS_PER_SEC)
			  / lextiming[j].calls;
	    printf("steps % 3d average % 12.1f clocks/run (% 5.1f min/run)\n",
		lextiming[j].steps,
		(double) lextiming[j].totalclocks /
		(double) lextiming[j].calls,
		rate/60);

	    #ifdef EXTRADEBUG1
	    printf("\ttotalclocks %ld, calls %d\n",
		lextiming[j].totalclocks, lextiming[j].calls);
	    #endif

	}
	#endif
}

/*
 * lexrun -- run lexpermute with top-level timing and print output code.  this
 * happens two different times in main(), so tidier to keep it in a subroutine
 */
void lexrun(LEXPERMUTE_ARGS *la)
{
    clock_t startsize, endsize;		/* timing variables */
    long long solspace = 1;		/* size of solution space */
    long long runsize;			/* number of permutations we plan to
					   do, less than or equal to solspace */
    unsigned long i;			/* counter */
    long long *iterations;		/* iterations processed by lexrun() */

    /* calculate the total number of permutations */
    for (i=0; i<la->steps; i++) {
	solspace = solspace * (SIZE-i);
    }

    printf("steps: %2d, solution space: %15.1f "SCALET", "
	   "estimate: %s\n", la->steps, (double) solspace/SCALE,
	   sec2string((long long)(solspace/GuessRate)));
    if (BeginIteration || StopAfter) {
	if (StopAfter) {
	    runsize = StopAfter;
	} else {
	    runsize = solspace - BeginIteration;
	}
	printf("constrained run size:      %15.1f "SCALET", "
	   "estimate: %s\n\n", (double) runsize/SCALE,
	   sec2string((long long)(runsize/GuessRate)));
    } else {
	puts("");
    }

    startsize=clock();
    if (la->steps == 7) {
	iterations = (long long *)lexpermute_seven(la);
    } else {
	iterations = (long long *)lexpermute(la);
    }
    endsize=clock();

    #ifdef DEBUGTIMING
    lextiming[(la->steps)-1].totalclocks += endsize - startsize;
    lextiming[(la->steps)-1].calls += 1;
    #endif

    double elapsed = (double)(endsize - startsize) / CLOCKS_PER_SEC;

    double rate = (double) *iterations / (elapsed * SCALE);
    printf("\nTotal time for sequence of length %d: "
	  "%.1f sec, rate %.1f million/sec\n", la->steps, elapsed, rate);
    printf("Completed permutations %20lld - %20lld, Results %2d\n",
	BeginIteration, BeginIteration + *iterations, Results);

    if (BeginIteration || StopAfter) {
	if (*iterations != runsize) {
	    printf("WARNING! iterations %lld not equal to \n"
		   "         runsize %lld - BeginIteration %lld\n",
		   *iterations,solspace,BeginIteration);
	}
    }
    else if (*iterations != solspace) {
	printf("WARNING! iterations %lld not equal to \n"
	       "         solspace   %lld\n",*iterations,solspace);
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
    
    long long *iterations = malloc(sizeof(long long));
    *iterations = 0;
			/* count the number we process internally -- use a
			 * dynamically allocated value for passing back via
			 * phtreads
			 */
    long mypuzzle;	/* local update of puzzle */
    int set_i;		/* set index pointer */
    register int j;	/* counter */
    char stemp[80];	/* temporary string array for printing things */

    if (a->steps == 1) {
	/* we have reached the end, check the result(s) & return */
	for (set_i=BeginStep[1]; set_i<a->setsize; set_i++) {
	    #ifdef EXTRADEBUG1
	    printf("final step, testing flip of pos %d\n", a->set[set_i]);
	    #endif
	    
	    *iterations += 1; /* */
	    mypuzzle = flipout(a->puzzle, a->set[set_i]);
	    if (debug > 1) {
		int ii = 0;
		int ts[SIZE];
		memcpy(&ts[0], a->solution, SIZE*sizeof(int));
		for (ii=0; ii < SIZE; ii++) {
		    if (ts[ii] == -1) {
			ts[ii] = a->set[set_i]; ts[ii+1] = -1; break;
		    }
		}
		int ti[Steps+1];
		long long tn = 0;
		sequence2index(ts, Steps, ti);
		index2permute(ti, Steps, tn);
		char temp[80];
		sprintf(temp, "finishing sequence %20lld: ", tn);
		printseq(stdout, temp, ts, SIZE, -1);
		if (debug > 2) {
		    printpuzzle(&mypuzzle);
		}
	    }
	    if (mypuzzle == PUZZLECOMPLETE) {
		printseq(stdout, "\nsuccess with sequence: ",
			 a->solution, SIZE, a->set[set_i]);
		printpuzzle(&mypuzzle);
		Results++;
	    }

	    /*
	     * we can no longer do spot check of timing & results here, as it
	     * requires updating a global which isn't a good idea if we're in
	     * a 'child thread'.  Do this now at parent thread level.
	     */
	}
	BeginStep[1]=0;
	return (void *) iterations;
    }

    /*
     * If we're at THREADLEVEL, then dispatch downstream lexpermute() calls in
     * parallel groups of THREADS.  Do some initialization here, and the
     * thread mucking in the main loop.
     */
    pthread_t me = pthread_self();
    short isthread = (! pthread_equal(tid_main, me));

    pthread_t mythreads[Threads];
    int mythreadi = -1;			/* active threads are 0..mythreadi */

    int mysetsize = a->setsize-1;
					/* ensure that each thread is working
					 * on its own array, not one shared
					 * between threads
					 */
    LEXPERMUTE_ARGS downstream[Threads];
					/* give each thread its own argument
					 * structure */

    #ifdef DEBUGTIMING
    clock_t startsize[Threads];		/* timer for each thread */
    #endif

    #ifdef EXTRADEBUG1
    sprintf(stemp, "--\n%sstarting lexpermute ", (isthread ? "*" : ""));
    print_lexargs(stemp,a);
    #endif

    /* if we're here, we have more than one step left to process */
    for (set_i=BeginStep[a->steps]; set_i<a->setsize; set_i++) {
	/* at the top level, announce the start of each main branch */
	if (a->solution[0] == -1) {
	    printf("Starting top-level element %d\n", a->set[set_i]);
	}

	/* update puzzle */
	mypuzzle = flipout(a->puzzle, a->set[set_i]);

	/* set threadlevel, even if we aren't threading here */
	if (a->steps == ThreadLevel) {
	    /* mythreadi contains the index of the last active mythread[] */
	    mythreadi++;
	    if (mythreadi >= Threads) {
		/* we shouldn't get here */
		fprintf(stderr, "Error counting threads\n"); exit(1);
	    }
	} else {
	    mythreadi=0;
	}

	/*
	 * line up arguments for downstream lexpermute call
	 */
	/* copy solutionpath and this element into structure */
	for (j = 0; j < SIZE; j++) {
	    if (a->solution[j] == -1) {
		downstream[mythreadi].solution[j] = a->set[set_i];
		downstream[mythreadi].solution[j+1] = -1;
		break;
	    }
	    /* else */
	    downstream[mythreadi].solution[j] = a->solution[j];
	}

	/* set up downstream set of moves */
	if (set_i>0) {
	    memcpy(&(downstream[mythreadi].set[0]),
		   &(a->set[0]),
		   set_i*sizeof(int)
		  );
	}
	if (set_i < mysetsize) {
	    memcpy(&(downstream[mythreadi].set[set_i]),
		   &(a->set[set_i+1]),
		   (mysetsize-set_i)*sizeof(int)
		  );
	}

	downstream[mythreadi].puzzle = mypuzzle;
	downstream[mythreadi].setsize = mysetsize;
	downstream[mythreadi].steps = (a->steps)-1;

	#ifdef EXTRADEBUG1
	sprintf(stemp, "stepsleft %d working elem %d, pass down set: ",
		a->steps, a->set[set_i]);
	printseq(stderr, stemp, downstream[mythreadi].set, mysetsize, -1);
	#endif

	if (a->steps == ThreadLevel) {
	    /*
	     * right, call multiple downstream lexpermutes via pthreads
	     */
	    #ifdef EXTRADEBUG1
	    sprintf(stemp, "set_i %d invoking thread %d to process ",
		    set_i, mythreadi);
	    print_lexargs(stemp, &downstream[mythreadi]);
	    #endif

	    void *(*lexfunction)(void *);
	    if (downstream[mythreadi].steps == 7) {
		lexfunction = lexpermute_seven;
	    } else {
		lexfunction = lexpermute;
	    }

	    #ifdef DEBUGTIMING
	    startsize[mythreadi]=clock();
	    #endif

	    if (pthread_create(&(mythreads[mythreadi]), NULL,
				 lexfunction, &downstream[mythreadi]))
	    {
		fprintf(stderr, "Error creating thread\n"); exit(1);
	    }
	    
	    /*
	     * now if we've used all our threads, or if we're at the end of i,
	     * collect the threads
	     */
	    if ((mythreadi == (Threads-1)) || ((set_i+1) == (a->setsize))) {
		int joini;
		void *v_subiterations;
		for (joini = 0; joini <= mythreadi; joini++) {
		    #ifdef EXTRADEBUG1
		    fprintf(stderr, "joining thread %d\n", joini);
		    #endif
		    if (pthread_join(mythreads[joini], &v_subiterations)) {
			fprintf(stderr, "Error joining thread\n");
		    }

		    #ifdef DEBUGTIMING
		    clock_t endsize=clock();
		    lextiming[(a->steps)-1].totalclocks +=
			endsize - startsize[mythreadi];
		    lextiming[(a->steps)-1].calls += 1;
		    #endif

		    /*
		     * permutecount is a progress-tracking global, whereas
		     * iterations is a count of how many iterations this call
		     * and its subcalls have made
		     */
		    permutecount += *((long long *) v_subiterations);

		    *iterations += *((long long *) v_subiterations);
		    free(v_subiterations);
		}
		mythreadi = -1;
	    } /* end collect threads */
	} else {
	    /* Non-threaded execution of downstream lexpermute() */
	    long long *subiterations;

	    #ifdef DEBUGTIMING
	    clock_t startsize=clock();
	    #endif

	    if (downstream[mythreadi].steps == 7) {
		subiterations=
		    (long long *)lexpermute_seven(&downstream[mythreadi]);
	    } else {
		subiterations=(long long *)lexpermute(&downstream[mythreadi]);
	    }

	    if ((a->steps == CheckLevel) && (ThreadLevel == NOTHREADS)) {
		permutecount += *subiterations;
	    }

	    #ifdef DEBUGTIMING
	    clock_t endsize=clock();
	    lextiming[(a->steps)-1].totalclocks += endsize - startsize;
	    lextiming[(a->steps)-1].calls += 1;
	    #endif

	    *iterations += *subiterations;
	    free(subiterations); /* release mem allocated by lexpermute() */
	}
	
	/* status check time? */
	if (permutecount > NextCountCheck) {
	    timecheck(a->solution, a->set[set_i+1]);
	    LastCountCheck = permutecount;
	    NextCountCheck = permutecount + CHECKINTERVAL;
	}

    } /* end iteration over the set */
    BeginStep[a->steps]=0; /* reset after first use */

    #ifdef EXTRADEBUG1
    if ((a->steps == (ThreadLevel-1)) && isthread) {
	sprintf(stemp, "Completed thread %ld iterations %ld, ",
	        (long) me, *iterations);
	print_lexargs(stemp,a);
    }
    #endif

    return (void *)iterations;

    /*
    ...and now startpoint can be any array, just process the steps in
    startpoint and call &lexpermute with that many fewer steps
    */
}

/* same as lexpermute(), but only works if steps==7, because this version does
 * no recursion.  Intended to be one less than THREADLEVEL
 */
void *lexpermute_seven(void *argstruct)
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

    if (a->steps != 7) {
	printf("lexpermute_seven called with steps=%d -- exiting\n",a->steps);
	exit(1);
    }

    /* long here, or long long? */
    long iterations = 0;
			/* count the number we process internally -- use a
			 * dynamically allocated value for passing back via
			 * phtreads
			 */
    long mypuzzle[8];	/* local update(s) of puzzle, levels1-7, 0 unused */
    int set_i;		/* set index pointer */
    register int j;	/* counter */
    char stemp[80];	/* temporary string array for printing things */

    unsigned long setmap = 0;
			/* This bitmap shows which elements of the set are
			 * already in use by another step in the permutation.
			 * i.e. if:
			 * set    = [1, 2, 3, 4, 5, 6, 7, 8, 9]
			 * and
			 * setmap=0b 1  1  1  1  0  0  0  0  0
			 * ...then we are at level steps=3 (7-4), possible
			 *    values still to use are 5-9.
			 *
			 * ...this used to be an array that looked like
			 * setmap = [6, 7, 5, 4, 0, 0, 0, 0, 0]
			 * but index[] already contains the sequence details
			 * and this should be faster.
			 */
    
    int index[8];	/* indexes for the various levels, 0 unused, 1-7 used */
    unsigned long move;	/* temporary bitmap of move being set/checked/cleared */

    /*
        at any given point, solution being tested is:
	a->solution, a->set[index[7]], a->set[index[6]], ... a->set[index[1]]
    */
    char solnstart[80];		/* make string of solution established before
				   this function, for use in debug/victory
				   output */
    sprintf(solnstart, "sequence: ");
    for(j=0; j < SIZE; j++) {
	if (a->solution[j] == -1) { break; }
	sprintf(stemp, "%d,", a->solution[j]);
	strcat(solnstart, stemp);
    }

    #ifdef EXTRADEBUG1
    pthread_t me = pthread_self();
    short isthread = (! pthread_equal(tid_main, me));

    sprintf(stemp, "--\n%sstarting lexpermute7 ", (isthread ? "*" : ""));
    print_lexargs(stemp,a);
    #endif
    
    /*
	now in seven loops, cycle through all the permutations of the
	steps/set passed to us
    */
    for (index[7] = BeginStep[7]; index[7] < a->setsize; index[7]++) {
      /* mark our place in the map -- at this level we don't need to worry
       * about stepping on any other level */
      move=(1 << index[7]);
      setmap |= move; /* set bit representing occupied move */

      /* update puzzle */
      mypuzzle[7] = flipout(a->puzzle, a->set[index[7]]);

      for (index[6] = 0; index[6] < a->setsize; index[6]++) {
	/* skip this index value if it is in use upstream */
	move=(1 << index[6]);
	if (setmap & move) { continue; }
	setmap |= move;

	mypuzzle[6] = flipout(mypuzzle[7], a->set[index[6]]);

	for (index[5] = 0; index[5] < a->setsize; index[5]++) {
	  /* skip this index value if it is in use upstream */
	  move=(1 << index[5]);
	  if (setmap & move) { continue; }
	  setmap |= move;

	  mypuzzle[5] = flipout(mypuzzle[6], a->set[index[5]]);

	  for (index[4] = 0; index[4] < a->setsize; index[4]++) {
	    /* skip this index value if it is in use upstream */
	    move=(1 << index[4]);
	    if (setmap & move) { continue; }
	    setmap |= move;

	    mypuzzle[4] = flipout(mypuzzle[5], a->set[index[4]]);

	    for (index[3] = 0; index[3] < a->setsize; index[3]++) {
	      /* skip this index value if it is in use upstream */
	      move=(1 << index[3]);
	      if (setmap & move) { continue; }
	      setmap |= move;

	      mypuzzle[3] = flipout(mypuzzle[4], a->set[index[3]]);

	      for (index[2] = 0; index[2] < a->setsize; index[2]++) {
		/* skip this index value if it is in use upstream */
		move=(1 << index[2]);
		if (setmap & move) { continue; }
		setmap |= move;

		mypuzzle[2] = flipout(mypuzzle[3], a->set[index[2]]);

		for (index[1]=0; index[1]<a->setsize; index[1]++)
		{
		    if (setmap & (1 << index[1])) { continue; }
		    /* no need to mark the set map here, no downstream */

		    mypuzzle[1] = flipout(mypuzzle[2], a->set[index[1]]);

		    iterations++;
		    if (mypuzzle[1] == PUZZLECOMPLETE) {
			int s[7];
			for (j=6; j>=0; j--) {
			    s[6-j] = a->set[index[j]];
			}
			sprintf(stemp, "success with %s", solnstart);
			printseq(stdout, stemp, s, 7, -1);
			printpuzzle(&mypuzzle[1]);
			Results++;
		    }
		    else if (debug > 1) {
			int ts[SIZE], ii, ti[Steps+1];
			long long tn = 0;
			for(j=0; j < SIZE; j++) {
			    if (a->solution[j] == -1) {
				for (ii=7; ii>0; ii--) {
				    ts[j+7-ii] = a->set[index[ii]];
				}
				ts[j+7] = -1;
				break;
			    }
			    ts[j] = a->solution[j];
			}
			sequence2index(ts, Steps, ti);
			index2permute(ti, Steps, tn);
			char temp[80];
			sprintf(temp, "finishing sequence %20lld: ", tn);
			printseq(stdout, temp, ts, SIZE, -1);
	
			if (debug > 2) {
			    printpuzzle(&mypuzzle[1]);
			}
		    }
		} /* end for index 1 */

		setmap &= ~(1 << index[2]); /* the tilde is a not */
	      } /* end for index 2 */

	      setmap &= ~(1 << index[3]);
	    } /* end for index 3 */

	    setmap &= ~(1 << index[4]);
	  } /* end for index 4 */

	  setmap &= ~(1 << index[5]);
	} /* end for index 5 */

	setmap &= ~(1 << index[6]);
      } /* end for index 6 */

      setmap &= ~(1 << index[7]);
    } /* end for index 7 */
    if (BeginStep[7]) {
	for (j=0; j<8; j++) { BeginStep[j]=0; }
    }

    #ifdef EXTRADEBUG1
    if ((a->steps == (ThreadLevel-1)) && isthread) {
	sprintf(stemp, "Completed lex7 thread %ld iterations %ld, ",
	        (long) me, iterations);
	print_lexargs(stemp,a);
    }
    #endif

    long *returnval = malloc(sizeof(long));
    *returnval = iterations;
    return (void *)returnval;
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
	printseq(stdout, "trying ", testseq[i], SIZE, -1);
	for (j=0; j<SIZE; j++) {
	    if (testseq[i][j] == -1) { break; }
	    printf("flip %d:\n", testseq[i][j]);

	    #ifdef FLIPMACRO
	    flipm(*puzzle, testseq[i][j]);
	    #else
	    long temppuzzle = flipout(*puzzle, testseq[i][j]);
	    *puzzle = temppuzzle;
	    #endif

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
void timecheck(int *solution, int laststep)
{
    clock_t t = clock();
    long count_since_last = permutecount - LastCountCheck;
    char temp[120];

    double rate = ((double) permutecount /
		   (SCALE * (double) (t-start) / CLOCKS_PER_SEC));
    printf("%ld iter, %0.1f sec "
	   "[%lld total, %.2f "SCALET" checks/sec]\n",
	   count_since_last,
	   (double) (t-lasttimecheck) / CLOCKS_PER_SEC,
	   permutecount, rate);
    sprintf(temp, "Permute# %lld, just completed all sol under: ",
	   BeginIteration + permutecount);
    printseq(stdout, temp, solution, Steps, laststep);

    lasttimecheck=t;
    if ((StopAfter) && (permutecount >= StopAfter)) {
	printf("\nStopping after %lld iterations, at iteration # %lld\n",
		permutecount, BeginIteration + permutecount);

	double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;
	double rate = (double) permutecount / (elapsed * SCALE);
	printf("Elapsed %.1f seconds, overall rate %.1f "SCALET"/sec\n",
	       elapsed, rate);
	printf("Completed permutations %20lld - %20lld, Results %2d\n",
	    BeginIteration, BeginIteration + permutecount, Results);

	exit(0);
    }
}

/* initialize the global pos2flip array.  we could do this more directly, but
 * this shows the logic and doesn't take that much time in the grand scheme of
 * things */
void init_pos2flip()
{
    register int i;

    for (i=0; i < SIZE; i++) {
	/* the flip mask is equivalent to the flip of an empty puzzle at the
	 * given position */
	pos2flip[i] = 0;
	flip(&pos2flip[i], i);
    }
}


/* Initialize the Factor[] array, to help convert between iteration numbers
   and sequences.  Algorithm:

  Right, if we have S elements, and we are looking at N steps, then there are
  S * (S-1) * ... (S-N-1) possible permutations, and iteration X represents:
    element int (X/((S-1) * ... * (S-N-1))) of S,
    element int [remainder of above / ((S-2) * ... * (S-N-1))
	of the set without the previous element in it

  Compute the factors below into factor[]
*/
void init_factor(int steps)
{
    int i, j; /* counters */

    /* variables for inserting notable powers of two into the factor display
     * output.  this triggers some fun oddities around using bitshift in C and
     * using 'long long' variables.
     */
    short nexttwo = 16;		/* print power of two */
    short lasttwo;		/* last power of two printed */
    unsigned long long twopower=(1 << nexttwo);
				/* store the result */

    /* compute the factors given the SIZE of the move set, and the *steps*
     * number of steps we're taking.  We're basically going backwards through
     * the computation of SIZE*SIZE-1*...*SIZE-STEPS+1
     *
     * e.g. for SIZE=20, steps=8,
     *      solution space is 20 * 19 * 18 * 17 * 16 * 15 * 14 * 13
     *      factor[1] = 13
     *      factor[2] = 13 * 14
     *      ...
     *      factor[8] = solution space = 13 * 14 * ... * 20
     */
    long long result=SIZE-steps+1;
    int nextstep=(result+1);
    for (i=1; i <= steps; i++) {
	Factor[i] = result;
	if (debug) {
	    if (result > twopower) {
		printf("     2**%d = %-20llu\n", nexttwo, twopower);
		lasttwo = nexttwo;
		nexttwo += 16;
		if (nexttwo > 61) { nexttwo = 61; }
		/* can't bit shift beyond 32 bits */
		for (j=0; j<(nexttwo - lasttwo); j++) { twopower *= 2; }
	    }
	    printf("factor[%2d] = %-20llu  # %12s\n", i, result,
		sec2string((long long)(result/GuessRate)));
	}
	result = result * nextstep;
	nextstep++;
    }
    if (nextstep != SIZE+2) {
	fprintf(stderr, "We got our math wrong. nextstep should be %d "
			 "but is %d\n", SIZE+2, nextstep);
	exit(1);
    }
}

/* print details around how to split the processing of all the permutations of
 * length "Steps" into more acceptable chunks
 */
void show_split()
{
    /* number of seconds that defines an acceptable chunk -- add more if we
     * haven't reached this limit, and use step levels that are less than this
     * limit */

    #define	ACCEPTABLE	7*3600
    int i;	/* counter */

    /* describe overall problem space */

    printf("steps: %d, solution space: %.2f "SCALET", estimate: %s\n\n",
	   Steps,
	   (double) Factor[Steps]/SCALE,
	   sec2string((long long)(Factor[Steps]/GuessRate)));

    /*
     * print starting iteration for each of the top steps, and estimated
     * duration to run
     */
    printf("%15s %20s  Duration in Hours\n", "TopLvlElement", "Iteration #");
    for (i=0; i < SIZE; i++) {
	printf("%15d %20lld  %.3f\n", moves[i], (long long) Factor[Steps-1]*i,
	    (float) (Factor[Steps-1]) / (GuessRate*3600));
    }
    puts(""); /* includes a CR */

    /*
     * find the most appropriate step level to split at
     */
    int splitfactor = 0;
    for (i=1; i<=Steps; i++) {
	if ((Factor[i]/GuessRate) > ACCEPTABLE) {
	    splitfactor = i-1;
	    printf("Splitting runs at steps=%d (%s) -- "
		   "(next level steps=%d is %s per run)\n"
		   "Total runs/chunks: %lld\n",
		   i-1, sec2string((long long)Factor[i-1]/GuessRate),
		   i,   sec2string((long long)Factor[i]/GuessRate),
		   Factor[Steps]/Factor[splitfactor]
		   );
	    break;
	}
    }

    long long nextStart = 0;	/* permutation number of next --begin arg */
    int thisChunk = 0;		/* of N chunks of size Factor[splitfactor],
				   this is number "thisChunk" */
    int thisIndex[Steps+1];
    /* int thisSequence[Steps]; */

    /* handle the starting case */
    if (BeginIteration) {
	/* We want to find the iteration that starts a call to
	 * lexpermute_seven.  Instead of a more complex method, try the
	 * nearest multiple of Factor[7].
	 */
	long long steps7_begin = Factor[7]*(BeginIteration/Factor[7]);

	/* find next toplevel starting iteration */
	thisChunk = BeginIteration/Factor[splitfactor];
	nextStart = Factor[splitfactor]*(1+thisChunk);

	long long first_iteration_perms = nextStart-steps7_begin;

	/* only add one more chunk to this first iteration if the runtime is
	 * less than ACCEPTABLE -- we don't want the first run to be 7 hours
	 * when the rest are 3 hours
	 */
	if ((first_iteration_perms/GuessRate) < ACCEPTABLE) {
	    nextStart += Factor[splitfactor];
	}

	/* get sequence we start with */
	permute2index(steps7_begin, Steps, thisIndex);
	/* index2sequence(thisIndex, Steps, thisSequence); */
	
	printf("weirdlabsolve -n %d --begin %lld -S %lld > Results/n%d.top%d\n",
	    Steps, steps7_begin, first_iteration_perms-1,
	    Steps, thisChunk);
	printf("# runsize is %.1f hours, ",
	       (first_iteration_perms/(GuessRate*3600)));
	revprintseq(stdout, "starting at index ", &thisIndex[1], Steps, -1);
    }
    while (nextStart < Factor[Steps]) {
	/* now we are dealing in clean chunks of size Factor[splitfactor] */
	thisChunk++;
	permute2index(nextStart, Steps, thisIndex);

	printf("weirdlabsolve -n %d --begin %lld -S %lld > Results/n%d.top%d\n",
	    Steps, nextStart, Factor[splitfactor]-1,
	    Steps, thisChunk);
	printf("# runsize is %.1f hours, ",
	    (Factor[splitfactor]/(GuessRate*3600)));
	revprintseq(stdout, "starting at index ", &thisIndex[1], Steps, -1);

	nextStart += Factor[splitfactor];
    }
}
	
/* if we've received a sigint, tell the lexpermutes to stop ASAP */
void signal_stop(int signal)
{
    printf("Caught signal %d, stopping ASAP\n", signal);
    NextCountCheck = StopAfter = 1;
}

/* if we've received an information signal, tell lexpermutes to report in */
void signal_info(int signal)
{
    printf("Caught signal %d, stats ASAP\n", signal);
    NextCountCheck = 1;
}

/*
 * now in macro form as flipm() above, better to use that
 */
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

    puzzle[HEIGHT*WIDTH]
    x+1, P+1 -- no for 4,9,14,19 P%WIDTH=WIDTH-1
    x-1, P-1 -- no for 0,5,10,15 P%WIDTH=0
    y+1, P+5 -- no for 15-19 P/WIDTH=HEIGHT-1
    y-1, P-5 -- no for 0-4 P/WIDTH=0

    a flip is an XOR -- ^ in C parlance
    */

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

/* this version of flip does not modify puzzle, but returns the flipped puzzle
 */

long flipout_function(long puzzle, int position)
{
    /* flip position itself */
    puzzle = puzzle ^ pos2bit[position];
    if ((position % WIDTH) < WIDTHLESS1) {
	puzzle = puzzle ^ pos2bit[position+1];
    }
    if ((position % WIDTH) > 0) {
	puzzle = puzzle ^ pos2bit[position-1];
    }
    if ((position / WIDTH) < HEIGHTLESS1) {
	puzzle = puzzle ^ pos2bit[position+WIDTH];
    }
    if ((position / WIDTH) > 0) {
	puzzle = puzzle ^ pos2bit[position-WIDTH];
    }

    return puzzle;
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
void printseq(FILE *stream, char *intro, int sequence[], int size, int extra)
{
    register int i;
    char out[160];	/* construct output into this string, then print */
    char tmp[160];	/* temporary for sprintf */

    strcpy(out, intro);

    if (sequence[0] == -1) {
	if (extra > 0) {
	    sprintf(tmp, "%d", extra);
	    strcat(out, tmp);
	} else {
	    strcat(out, "(null sequence)");
	}
    }
    else {
	/* don't print a trailing comma at the end */
	sprintf(tmp, "%d", sequence[0]);
	strcat(out, tmp);
	for(i = 1; i < size; i++) {
	    if (sequence[i] == -1) { break; }
	    sprintf(tmp, ",%d", sequence[i]);
	    strcat(out, tmp);
	}
	if (extra > 0) {
	    sprintf(tmp, ",%d", extra);
	    strcat(out, tmp);
	}
    }
    fprintf(stream, "%s\n", out);
}

/* print out a sequence in reverse -- as above*/
void revprintseq(FILE *stream, char *intro, int sequence[], int size, int extra)
{
    int ts[size], i;
    for (i=size-1; i >= 0; i--) {
	ts[size-1-i] = sequence[i];
    }
    printseq(stream, intro, ts, size, extra);
}

/* dump the values of a lexpermute run structure */
void print_lexargs(char *intro, LEXPERMUTE_ARGS *a)
{
    char out[400];
    char tmp[160];
    register short i;

    strcpy(out, intro);

    sprintf(tmp, "steps %2d, puzzle 0x%8lX, setsize %2d\n\tsolution:  ",
		 a->steps, a->puzzle, a->setsize);
    strcat(out, tmp);

    if (a->solution[0] == -1) {
	strcat(out, "(no solution path yet)");
    }
    else {
	sprintf(tmp, "\t%d", a->solution[0]);
	strcat(out, tmp);
	for(i=1; i<SIZE; i++) {
	    if (a->solution[i] == -1) { break; }
	    sprintf(tmp, ",%d", a->solution[i]);
	    strcat(out, tmp);
	}
    }
    sprintf(tmp, "\n\tmoves set: %d", a->set[0]);
    strcat(out, tmp);
    for(i=1; i < a->setsize; i++) {
	sprintf(tmp, ",%d", a->set[i]);
	strcat(out, tmp);
    }
    fprintf(stderr, "%s\n", out);
}

/* take a number in seconds, return a string value that represents the value
 * at the appropriate scale
 */
char *sec2string(long long seconds)
{
    char tmp[400];

    if (seconds < 60) {
	sprintf(tmp, "%lld sec  ", seconds);
    }
    else if (seconds < 3600) {
	sprintf(tmp, "%2.1f min  ", (float)seconds/60);
    }
    else if (seconds < 86400) {
	sprintf(tmp, "%2.1f hours", (float)seconds/3600);
    }
    else if (seconds < 604800) {
	sprintf(tmp, "%2.1f days ", (float)seconds/86400);
    }
    else if (seconds < 31536000) {
	sprintf(tmp, "%2.1f weeks", (float)seconds/604800);
    }
    else {
	sprintf(tmp, "%.1f years", (float)seconds/31536000);
    }

    char *returnval = malloc(strlen(tmp));
    strcat(returnval, tmp);
    return returnval;
}
