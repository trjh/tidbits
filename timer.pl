#!/usr/bin/perl -w

use strict;
use Scalar::Util qw(looks_like_number);

use Term::Cap;
use Term::ReadKey;

#
my $USAGE = "
  maintain a set of running timers

  timer [mm|NNm|NNh|hh:mm [text]] -- set a timer for (time listed) from now
  timer [NNNu [text]] -- set a timer for explicit unix timestamp NNN

  timer [-l] -- list existing timers (as stored in ~/.timer)

  timer -k x[,y,z]	-- remove timer entries
  timer -n		-- list by order entered

  timer -u <time> x
  timer -u x <time>	-- update entry X with time <time>
			   time *must* have a suffix or :

  timer -ut x <txt> [foo] -- update entry X with new text

  timer -r		-- print raw timestamps
  timer -o		-- list by city order (Fornax 1st, then O)
  ...   -d		-- increase debug level
  ...   -wo		-- rewrite timer linst in city order
  ...   -h		-- highlight overdue items
  ...   -H		-- do not highlight overdue items

  ...   --help		-- usage
";
#

my $TimerFile = $ENV{'HOME'}."/.timer";

my $now = time();
my $List = 0;			# list timers? [0 no, 1 yes, 2 by order,
				#               3 city order]
my ($Expires,$Text) = (0,"");	# set a new timer
my @Kill = ();			# entries to remove
my $Update = -1;		# update an entry
my $debug = 1;
my $Raw = 0;
my $WriteOrder = 0;		# if 1, rewrite file in city order
my $FileUpdate = 0;		# if 0, don't update; 1, append; 2, rewrite

my ($CL, $HL, $N) = ("","","");
# if set, will be used to highlight text

while (@ARGV) {
    $_ = shift @ARGV;

    /^\d[\d\.:]*[mhu]?$/ &&	do {
	if (@ARGV) {
	    # rest of the arguments are the text -- unless
	    # we hit a flag, in which case, continue
	    while (@ARGV) {
		if ($ARGV[0] =~ /^-./) {
		    last;
		}
		$Text .= (shift @ARGV)." ";
	    }
	}
	$Text =~ s/\s+$//;
	$Expires = &settime($_);
	next;
    };

    /^-k/ && 	do {
	my $klist = shift @ARGV;
	die $USAGE unless (defined($klist));
	$klist =~ s/(\d+)-(\d+)/join(",",$1 ... $2)/eg;
	push @Kill, (split (/,/,$klist));
	next;
    };

    /^-(d+)/ &&	do { $debug+=length($1); next };
    /^-l/ &&	do { $List=1; next };
    /^-n/ &&	do { $List=2; next };
    /^-o/ &&	do { $List=3; unshift @ARGV, "-h"; next };
    /^-r/ &&	do { $Raw=1; $List = ($List ? $List : 1); next };
    /^-wo/ &&	do { $WriteOrder=1; next };

    /^-ut/ &&	do {
	die "not enough arguments for -ut\n" unless ($#ARGV>0);
	$Update = shift(@ARGV);
	die "bad argument for -ut ($Update)\n"
	    unless (looks_like_number($Update));

	$Expires = "";
	while (@ARGV) {
	    if ($ARGV[0] =~ /^-./) {
		last;
	    }
	    $Expires .= (shift @ARGV)." ";
	}
	next;
    };
	
    /^-u/ &&	do {
	die "not enough arguments for -u\n" unless ($#ARGV>0);

	# find the argument that is the line to update, assume then that the
	# other is the time

	die "ambiguous arguments for -u ($ARGV[0] $ARGV[1])\n"
	    if (($ARGV[0] =~ /^\d+$/) and ($ARGV[1] =~ /^\d+$/));

	# which one is a whole number?

	for (my $i=0; $i<=1; $i++) {
	    if ($ARGV[$i] =~ /^\d+$/) {
		$Update = $ARGV[$i]; $Expires = $ARGV[not $i];
		last;
	    }
	}

	# ok which one is a letter?

	if ($Update == -1) {
	    for (my $i=0; $i<=1; $i++) {
		if ($ARGV[$i] =~ /^[A-Z]/) {
		    $Update = $ARGV[$i]; $Expires = $ARGV[not $i];
		    last;
		}
	    }
	}

	if ($Update eq "-1") {
	    die "bad argument for -u ($Update)\n";
	}
	    
	$Expires = &settime($Expires);
	splice (@ARGV,0,2);
	next;
    };

    /^-h/	&& do {
	my $OSPEED=9600;
	my $terminal = Term::Cap->Tgetent({OSPEED=>$OSPEED});
	# if we don't set OSPEED, we get a warning

	$CL = $terminal->Tputs('cl'); # Clear
	$HL = $terminal->Tputs('mr'); # Highlight
	$N  = $terminal->Tputs('me'); # Normal

	next;
    };

    /^-H/	&& do { $CL = $HL = $N = ""; next; };

    die $USAGE;
}
unless (@Kill or $Expires) { $List=($List ? $List : 1) }
if (($Update ne "-1") or @Kill or $WriteOrder) {
    $FileUpdate = 2;
} elsif ($Expires) {
    $FileUpdate = 1;
}

# read in existing timers
my @TimerList = ();
my %citysort = (); # see definition below
if (-f $TimerFile) {
    open (IN, $TimerFile) or die "can't open $TimerFile: $!\n";
    while (<IN>) {
	chomp;
	if (/^(\d+)\s+(.*?)\s*$/) {
	    push @TimerList, [$1,$2];
	    print ">> read entry ".$#TimerList." - $1 :: $2\n"
		if ($debug>1);
	    $citysort{&sortkey($2)} = $#TimerList;
	}
	# ignore comments
    }
    close (IN);
}

# process kill
if (@Kill) {
    foreach my $e (sort {$b <=> $a} @Kill) {
	next unless ($e <= $#TimerList);
	my $gone = splice(@TimerList,$e,1);
	print ">  remove entry ".$e." - ".($gone->[0])." :: ".($gone->[1])."\n"
		if ($debug>0);
    }
}

# process addition
if ($Expires) {
    if ($Update ne "-1") {
	if ($Update =~ /^[A-Z]/) {
	    # look for the entry number
	    if (defined($citysort{"111 $Update"})) {
		die "more than one match for city $Update\n"
		    if (defined($citysort{"111 ".$Update."_"}));
		$Update = $citysort{"111 $Update"};
	    }
	    elsif (defined($citysort{"555 $Update"})) {
		die "more than one match for city $Update\n"
		    if (defined($citysort{"555 ".$Update."_"}));
		$Update = $citysort{"555 $Update"};
	    }
	    else {
		die "can't find city matching $Update";
	    }
	}

	die "there is no $Update entry ($#TimerList)\n"
	    unless ($#TimerList >= $Update);
	my $ra = $TimerList[$Update];

	if (looks_like_number($Expires)) {
	    print ">  change entry ".$Update." - ".($ra->[0])." -> ".
		  $Expires." :: ".$ra->[1]."\n"
		if ($debug>0);
	    $ra->[0] = $Expires;
	} else {
	    print ">  change entry ".$Update." - ".($ra->[0])." :: ".
		  ($ra->[1])." -> ".$Expires."\n"
		if ($debug>0);
	    $ra->[1] = $Expires;
	}
    }
    else {
	push @TimerList, [$Expires,$Text];
	print ">  set entry ".$#TimerList." - ".$Expires." :: ".$Text."\n"
		if ($debug>0);
    }
}

# build some useful hashes, now that all our changes are done
my %TL = ();	   # key: time of expiry, value: index into $TimerList
   %citysort = (); # key: text modified to be city-sort order
		   # value: index into $TimerList

for (my $i=0; $i <= $#TimerList; $i++) {
    my ($tltime, $tltext) = @{$TimerList[$i]};
    # %TL entry
    while (defined($TL{$tltime})) {
	$tltime++
    }
    $TL{$tltime} = $i;

    # we defined this above for the -u argument,
    # but re-set in case something has changed
    $citysort{&sortkey($tltext)} = $i;
}

# if we are going to rewrite file in city order, change ordering now
if ($WriteOrder) {
    my @oldtimerlist = @TimerList;
    @TimerList = ();

    foreach my $k (sort keys %citysort) {
	push @TimerList, $oldtimerlist[$citysort{$k}];
    }

    # redo %TL as indexes have changed
    %TL = ();
    for (my $i=0; $i <= $#TimerList; $i++) {
	my ($tltime, $tltext) = @{$TimerList[$i]};
	# %TL entry
	while (defined($TL{$tltime})) {
	    $tltime++
	}
	$TL{$tltime} = $i;
    }
}

# use Data::Dumper;
# print Dumper(\@TimerList)."\n\n".Dumper(\%TL)."\n";
# exit;

# do whatever write is right
if ($FileUpdate>1) {
    # rewrite the whole file
    open (OUT, "> $TimerFile") or die "can't open/write $TimerFile: $!\n";
    for (my $i=0; $i <= $#TimerList; $i++) {
	print OUT $TimerList[$i]->[0]."\t".$TimerList[$i]->[1]."\n";
    }
    close OUT or die "error closing/write $TimerFile: $!\n";
}
elsif ($FileUpdate) {
    # just a new one
    open (OUT, ">> $TimerFile") or die "can't open/append $TimerFile: $!\n";
    print OUT $Expires."\t".$Text."\n";
    close OUT or die "error closing/append $TimerFile: $!\n";
}

# finally -- list?

my $ListOneText = ""; 	# if we list this way, write it to .timer
if ($List) {
    if ($debug) { print "\n" }

    unless (@TimerList) {
	print "no timers\n";
	exit;
    }

    # ordering by time remaining
    my @sorted = map { $TL{$_} } sort keys %TL;

    if ($List == 2) {
	# ordering by entry
	@sorted = (0 ... $#TimerList);
    }
    if ($List == 3) {
	# ordering by city name
	@sorted = ();
	foreach my $e (sort keys %citysort) {
	    push @sorted, $citysort{$e};
	}
    }
    my $overdue = 0;
    if ($TimerList[$sorted[0]]->[0] < $now) {
	print "$HL--- overdue ---$N\n" if ($List == 1);
	$overdue++;
    }
    my $daysaway = 0;
    foreach my $e (@sorted) {
	my ($tltime, $tltext) = @{$TimerList[$e]};

	if ($overdue) {
	    if ($tltime >= $now) {
		print "\n--- running ---\n" if ($List == 1);
		$overdue=0;
	    }
	}
	my @lt = localtime($tltime);
	my ($timedelta, $hhmmss) = ("","");

	if (abs($tltime-$now) < 60) {
	    $timedelta = sprintf "*** % 2.0fm", ($tltime-$now)/60;
	} else {
	    $timedelta = sprintf "%7s", &tp($tltime-$now);
	    my $tldeltadays = int(($tltime-$now)/(3600*24));
	    if ($tldeltadays > $daysaway) {
		print "--- + $tldeltadays day(s)\n";
		$daysaway = $tldeltadays;
	    }
	}
	if ($Raw) {
	    $hhmmss = sprintf "   %d", $tltime;
	} else {
	    $hhmmss = sprintf "   %2d:%02d:%02d", @lt[2,1,0];
	}
	printf "%s   %s   %s%2d. %s%s\n",
	    $timedelta, $hhmmss,
	    (($tltime-$now)<30 ? $HL : ""), $e, $tltext,
	    (($tltime-$now)<30 ? $N  : "");

	$ListOneText .= sprintf "# %2d:%02d:%02d   %s%2d. %s%s\n",
		@lt[2,1,0],
		(($tltime-$now)<30 ? $HL : ""), $e, $tltext,
		(($tltime-$now)<30 ? $N  : "");
    }

    if ($ListOneText && ($FileUpdate>1)) {
	open (OUT, ">> $TimerFile") or die "can't open/append $TimerFile: $!\n";
	print OUT $ListOneText."\n";
	close OUT or die "error closing/append $TimerFile: $!\n";
    }
}

##
## end
##
exit 0;

## tp($seconds) -- return a time string as SSs, MMm, or HH:MM

sub tp($) {
    my $seconds = shift @_;

    return ($seconds."s") if (abs($seconds) < 60);

    return (sprintf("%.0fm",($seconds/60))) if (abs($seconds) < 3600);

    my $neg = 0;
    if ($seconds < 0) { $neg++; $seconds *= -1 }

    my $sec = $seconds % 60;
    $seconds -= $sec;
#print "sec: $sec seconds: $seconds\n";
    my $minutes = ($seconds/60)%60;
    $seconds -= $minutes*60;
#print "min: $minutes seconds: $seconds\n";
    my $hours = $seconds/3600;
#print "hours: $hours\n";

    return (sprintf "%s%d:%02d",($neg?"-":""),$hours,$minutes);
}

# take a time argument, return an absolute time value based on $now
sub settime($) {
    my $in = shift @_;

    $in =~ /^(\d+)$/		&& do { return($now + $1*60) };
    $in =~ /^(\d+)m$/i		&& do { return($now + $1*60) };
    $in =~ /^(\d+)[:\.](\d+)$/	&& do { return($now + $1*3600 + $2*60) };
    $in =~ /^(\d+)h$/		&& do { return($now + $1*3600) };
    $in =~ /^(\d+)u$/		&& do { return($1) };

    die "UNKNOWN TIME FORMAT: $in\n";
    return $in;
}

# take a text string, return the citysort key based on it
sub sortkey($) {
    my $t = shift(@_);

    my $city = "";
    my $sortkey = "";
    if ($t =~ /([A-Z]\w*)/) {
	$city = $1;
    } else {
	warn "can't find a city in text: $t (expecting [A-Z]\\w*)\n";
	return ("");
    }

    print ">> $t -> city $city\n" if ($debug>1);
    if ($city =~ /q$/) {
	$sortkey = "555 $city";
    } else {
	$sortkey = "111 $city";
    }
    while (defined($citysort{$sortkey})) {
	$sortkey .= "_";
    }
    print ">> sortkey $t -> $sortkey\n" if ($debug>1);
    return ($sortkey);
}
