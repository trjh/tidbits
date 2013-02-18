#!/usr/bin/perl -w

use strict;

#
my $USAGE = "

  watch ~/.timers, beep and print a message when a timer is due

  maintain a set of running timers
  timer [mm|NNm|NNh|hh:mm [text]] -- set a timer for (time listed) from now
  timer [-l] -- list existing timers (as stored in ~/.timer)
  timer -k x[,y,z] -- remove timer entries
";
#

my $TimerFile = $ENV{'HOME'}."/.timer";
my $SDIR = "/System/Library/Sounds";

my $now = time();
my $List = 0;			# list timers?
my ($Expires,$Text) = (0,"");	# set a new timer
my @Kill = ();			# entries to remove
my $debug = 0;

my $lastTimerFilemtime = 0;
my $lastTimerFilecheck = 4; # count from 0-3
my %TimerList = ();	    # key: time, value: message
my @sounds = ();		# sounds
my %Beeped = ();		# timers we've already beeped on

# get sounds
{
    opendir(SND, $SDIR) or die "can't opendir $SDIR: $!\n";

    @sounds = grep { /\.aiff$/ } readdir(SND);

    closedir SND;
}

while (1) {
    # refresh .timers every 60 sec
    if ($lastTimerFilecheck > 3) {
	unless ((stat($TimerFile))[9] == $lastTimerFilemtime) {
	    $lastTimerFilemtime = (stat($TimerFile))[9];

	    %TimerList = ();
	    open (IN, $TimerFile) or die "can't open $TimerFile: $!\n";
	    while (<IN>) {
		chomp;
		if (/^(\d+)\s+(.*)$/) {
		    my ($time, $txt) = ($1,$2);
		    next if (($time - $now) < -60);
		    $TimerList{$time} = $txt;
		    print ">> read entry - $time :: $txt\n"
			if ($debug>1);
		}
	    }
	    close (IN);
	    my $k = keys %TimerList;
	    print "> updated (".$k." entries)\n";
	}
    } else {
	$lastTimerFilecheck++;
    }

    # anything to beep about?
    foreach my $e (sort {$a <=> $b} keys %TimerList) {
	my $delta = $e - $now;
	last if ($delta > 60);
	next if (defined($Beeped{$e}));
	if (abs($delta) < 30) {
	    my $sndi = int(rand($#sounds + 1));
	    system("/usr/bin/afplay $SDIR/$sounds[$sndi] 2>/dev/null");
	    print "\n*** ".localtime($now)." ".$TimerList{$e}."\n";

	    $Beeped{$e}++;
	}
    }

    # sleep
    sleep 10;
    $now = time();
}
