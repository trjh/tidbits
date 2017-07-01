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
static char *Version = "1.8.2_NOFLIPMACRO";

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>
#include <string.h>
#include <pthread.h>

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

/* #define EXTRADEBUG1 -- print more checkpoint details */
/* #define EXTRADEBUG2 -- print debug steps from flip() */
/* #define DEBUGTIMING -- track timings of execution at each level of
			  recursion */
/* #define FLIPMACRO	* use macro for inline flipping */

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

/* value to determine where to thread, and which threads are complete */
#define THREADLEVEL 8		/* do threading if steps == THREADLEVEL */
/* #define THREADS 4	*/	/* define # of threads here for now */
int Threads = 4;	
pthread_t tid_main;		/* thread id of main */

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
#define GUESSRATEHOURS GUESSRATE*3600	/* last # iterations/hour */

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
long flipout(long puzzle, int position);
void testmode(long *puzzle);
void lexrun(LEXPERMUTE_ARGS *la);
void *lexpermute(void *argstruct);
void *lexpermute_seven(void *argstruct);
void printseq(FILE *stream, char *, int sequence[], int size, int extra);
void timecheck();
void print_lexargs(char *intro, LEXPERMUTE_ARGS *a);

/* macro version of flip -- works on a long directly, not the pointer */
#define flipm(puzzle, pos) { \
    puzzle = puzzle ^ pos2bit[pos]; \
    if ((pos % WIDTH) < WIDTHLESS1)  { puzzle= puzzle ^ pos2bit[pos+1]; }; \
    if ((pos % WIDTH) > 0)           { puzzle= puzzle ^ pos2bit[pos-1]; }; \
    if ((pos / WIDTH) < HEIGHTLESS1) { puzzle= puzzle ^ pos2bit[pos+WIDTH];} \
    if ((pos / WIDTH) > 0)	     { puzzle= puzzle ^ pos2bit[pos-WIDTH];} \
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
	int steps = SIZE;
	tid_main = pthread_self();

	/* initialize puzzle and timers */
	resetpuzzle(&puzzle);
	start=lasttimecheck=clock();

	/* process options */
	int flags, opt;
	extern char *optarg;
	while ((opt = getopt(argc, argv, "dtvj:n:S:")) != -1) {
	    switch(opt) {
	    case 'd':
		debug++;
		break;
	    case 't':
		testmode(&puzzle);
		exit(0);
	    case 'v':
		printf("Version %s\n", Version);
		exit(0);
	    case 'n':
		steps = atoi(optarg);
		break;
	    case 'j':
		Threads = atoi(optarg);
		break;
	    case 'S':
		StopAfter = atoi(optarg);
		NextCountCheck = StopAfter;
		break;
	    default: /* '?' */
		fprintf(stderr, "Usage: %s [-d] [-t] [-n steps]\n"
				"\t-d	: increase debug value\n"
				"\t-t	: test mode -- just show details of "
					 "processing 5 sequences of 20\n"
				"\t-j T : number of threads to use\n"
				"\t-n N	: examine all solutions with N steps\n"
				"\t-S M	: stop after M iterations\n",
				argv[0]);
		exit(EXIT_FAILURE);
	    }
	}

	printf("Version %s\n", Version);
	if (debug>0) {
	    printf("debug level: %d\n", debug);
	}

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
	   (((double) solspace) / ((double) GUESSRATEHOURS)));

    startsize=clock();
    if (la->steps == 7) {
	iterations = (long *)lexpermute_seven(la);
    } else {
	iterations = (long *)lexpermute(la);
    }
    endsize=clock();

    #ifdef DEBUGTIMING
    lextiming[(la->steps)-1].totalclocks += endsize - startsize;
    lextiming[(la->steps)-1].calls += 1;
    #endif

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
    int set_i;		/* set index pointer */
    register int j;	/* counter */
    char stemp[80];	/* temporary string array for printing things */

    if (a->steps == 1) {
	/* we have reached the end, check the result(s) & return */
	for (set_i=0; set_i<a->setsize; set_i++) {
	    #ifdef EXTRADEBUG1
	    printf("final step, testing flip of pos %d\n", a->set[set_i]);
	    #endif
	    
	    *iterations += 1; /* */
	    mypuzzle = flipout(a->puzzle, a->set[set_i]);
	    if (debug > 1) {
		printseq(stdout, "finishing sequence: ",
			 a->solution, SIZE, a->set[set_i]);
		if (debug > 2) {
		    printpuzzle(&mypuzzle);
		}
	    }
	    if (mypuzzle == PUZZLECOMPLETE) {
		printseq(stdout, "\nsuccess with sequence: ",
			 a->solution, SIZE, a->set[set_i]);
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
    for (set_i=0; set_i<a->setsize; set_i++) {
	/* at the top level, announce the start of each main branch */
	if (a->solution[0] == -1) {
	    printf("Starting top-level element %d\n", a->set[set_i]);
	}

	/* update puzzle */
	mypuzzle = flipout(a->puzzle, a->set[set_i]);

	/* set threadlevel, even if we aren't threading here */
	if (a->steps == THREADLEVEL) {
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

	if (a->steps == THREADLEVEL) {
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

		    permutecount += *((long *) v_subiterations);
		    if (permutecount > NextCountCheck) {
			timecheck();
			LastCountCheck = permutecount;
			NextCountCheck = permutecount + CHECKINTERVAL;
		    }
		    *iterations += *((long *) v_subiterations);
		    free(v_subiterations);
		}
		mythreadi = -1;
	    } /* end collect threads */
	} else {
	    /* Non-threaded execution of downstream lexpermute() */
	    long *subiterations;

	    #ifdef DEBUGTIMING
	    clock_t startsize=clock();
	    #endif

	    if (downstream[mythreadi].steps == 7) {
		subiterations=(long *)lexpermute_seven(&downstream[mythreadi]);
	    } else {
		subiterations=(long *)lexpermute(&downstream[mythreadi]);
	    }

	    #ifdef DEBUGTIMING
	    clock_t endsize=clock();
	    lextiming[(a->steps)-1].totalclocks += endsize - startsize;
	    lextiming[(a->steps)-1].calls += 1;
	    #endif

	    *iterations += *subiterations;
	    free(subiterations);	/* don't need memory allocated in that call */
	}

    } /* end iteration over the set */

    #ifdef EXTRADEBUG1
    if ((a->steps == (THREADLEVEL-1)) && isthread) {
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

    long *iterations = malloc(sizeof(long));
    *iterations = 0;
			/* count the number we process internally -- use a
			 * dynamically allocated value for passing back via
			 * phtreads
			 */
    long mypuzzle[8];	/* local update(s) of puzzle, levels1-7, 0 unused */
    int set_i;		/* set index pointer */
    register int j;	/* counter */
    char stemp[80];	/* temporary string array for printing things */

    int setmap[a->setsize];
			/* This array shows which elements of the set are
			 * already in use by another step in the permutation.
			 * i.e. if:
			 * set    = [1, 2, 3, 4, 5, 6, 7, 8, 9]
			 * and
			 * setmap = [6, 7, 5, 4, 0, 0, 0, 0, 0]
			 * ...then we are at level steps=3, possible values
			 *    still to use are 5-9, and the permutation being
			 *    tested starts with 2, 1, 3, 4
			 */
    for (j=0; j<a->setsize; j++) { setmap[j] = 0; }
    
    int index[8];	/* indexes for the various levels, 0 unused, 1-7 used */

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
    for (index[7] = 0; index[7] < a->setsize; index[7]++) {
      /* mark our place in the map -- at this level we don't need to worry
       * about stepping on any other level */
      setmap[index[7]] = 7;

      /* update puzzle */
      #ifdef FLIPMACRO
      mypuzzle[7] = a->puzzle;
      flipm(mypuzzle[7], a->set[index[7]]);
      #else
      mypuzzle[7] = flipout(a->puzzle, a->set[index[7]]);
      #endif

      for (index[6] = 0; index[6] < a->setsize; index[6]++) {
	/* skip this index value if it is in use upstream */
	if (setmap[index[6]] != 0) { continue; }
	setmap[index[6]] = 6;

	#ifdef FLIPMACRO
	mypuzzle[6] = mypuzzle[7];
	flipm(mypuzzle[6], a->set[index[6]]);
	#else
	mypuzzle[6] = flipout(mypuzzle[7], a->set[index[6]]);
	#endif

	for (index[5] = 0; index[5] < a->setsize; index[5]++) {
	  /* skip this index value if it is in use upstream */
	  if (setmap[index[5]] != 0) { continue; }
	  setmap[index[5]] = 5;

	  #ifdef FLIPMACRO
	  mypuzzle[5] = mypuzzle[6];
	  flipm(mypuzzle[5], a->set[index[5]]);
	  #else
	  mypuzzle[5] = flipout(mypuzzle[6], a->set[index[5]]);
	  #endif

	  for (index[4] = 0; index[4] < a->setsize; index[4]++) {
	    /* skip this index value if it is in use upstream */
	    if (setmap[index[4]] != 0) { continue; }
	    setmap[index[4]] = 4;

	    #ifdef FLIPMACRO
	    mypuzzle[4] = mypuzzle[5];
	    flipm(mypuzzle[4], a->set[index[4]]);
	    #else
	    mypuzzle[4] = flipout(mypuzzle[5], a->set[index[4]]);
	    #endif

	    for (index[3] = 0; index[3] < a->setsize; index[3]++) {
	      /* skip this index value if it is in use upstream */
	      if (setmap[index[3]] != 0) { continue; }
	      setmap[index[3]] = 3;

	      #ifdef FLIPMACRO
	      mypuzzle[3] = mypuzzle[4];
	      flipm(mypuzzle[3], a->set[index[3]]);
	      #else
	      mypuzzle[3] = flipout(mypuzzle[4], a->set[index[3]]);
	      #endif

	      for (index[2] = 0; index[2] < a->setsize; index[2]++) {
		/* skip this index value if it is in use upstream */
		if (setmap[index[2]] != 0) { continue; }
		setmap[index[2]] = 2;

		#ifdef FLIPMACRO
	        mypuzzle[2] = mypuzzle[3];
	        flipm(mypuzzle[2], a->set[index[2]]);
		#else
		mypuzzle[2] = flipout(mypuzzle[3], a->set[index[2]]);
		#endif

		for (index[1]=0; index[1]<a->setsize; index[1]++)
		{
		    if (setmap[index[1]] != 0) { continue; }
		    /* no need to mark the set map here, no downstream */

		    #ifdef FLIPMACRO
		    mypuzzle[1] = mypuzzle[2];
		    flipm(mypuzzle[1], a->set[index[1]]);
		    #else
		    mypuzzle[1] = flipout(mypuzzle[2], a->set[index[1]]);
		    #endif

		    *iterations += 1;
		    if (mypuzzle[1] == PUZZLECOMPLETE) {
			int s[7];
			for (j=6; j>=0; j--) {
			    s[6-j] = a->set[index[j]];
			}
			sprintf(stemp, "success with %s", solnstart);
			printseq(stdout, stemp, s, 7, -1);
			printpuzzle(&mypuzzle[1]);
		    }
		    else if (debug > 1) {
			int s[7];
			for (j=7; j>0; j--) {
			    s[7-j] = a->set[index[j]];
			}
			sprintf(stemp, "finishing %s", solnstart);
			printseq(stdout, stemp, s, 7, -1);
			if (debug > 2) {
			    printpuzzle(&mypuzzle[1]);
			}
		    }
		} /* end for index 1 */

		setmap[index[2]] = 0;
	      } /* end for index 2 */

	      setmap[index[3]] = 0;
	    } /* end for index 3 */

	    setmap[index[4]] = 0;
	  } /* end for index 4 */

	  setmap[index[5]] = 0;
	} /* end for index 5 */

	setmap[index[6]] = 0;
      } /* end for index 6 */

      setmap[index[7]] = 0;
    } /* end for index 7 */

    #ifdef EXTRADEBUG1
    if ((a->steps == (THREADLEVEL-1)) && isthread) {
	sprintf(stemp, "Completed lex7 thread %ld iterations %ld, ",
	        (long) me, *iterations);
	print_lexargs(stemp,a);
    }
    #endif

    return (void *)iterations;
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
	    printpuzzle(&temppuzzle);
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

long flipout(long puzzle, int position)
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
