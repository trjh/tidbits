#!/usr/bin/perl -w

#
# md5allupdate.pl
#
my $USAGE = "$0 [-f md5file] [-w] path ...
    -f md5file		read initial set of checksums from <md5file>
    -w			only warn, don't die, on read errors
    -L			follow symbolic links, as with find -L
    -e pattern		ignore files in path that match pattern (more than one
			may be specified)
";
#
# compute md5 checksums on all files in path.  if md5file is specified, read
# sums & paths from that file first.  when perusing path, if a file is found
# that has a sum in md5file and has not changed since md5file was written, do
# not generate a sum for it.
#
# e.g.
# cd /cygdrive/c; mv md5-all-c.txt.bz2 md5-all-c.txt.bz2-
# ~/Proj/bin/md5allupdate.pl -w -f md5-all-c.txt.bz2- . > md5-all-c.txt
#

use strict;
use Digest::MD5;
use File::Find;

my $MD5file = "";
my $MD5time = 0;
my %seenfile = ();	# files already seen, inside $MD5file
my @Sources = ();
my @ExceptPattern = ();
my $Debug = 0;
my $warnonly = 0;	# warn, don't fail, on file read errors
my $start = time();
my $lasttime = $start;
my $alreadyhad = 0;
my $md5generated = 0;
my $md5format = 0;	# which format to use?
    # 0: /^MD5 \((.*)\) = [a-f0-9]{32}\s*$/)
    # 1: /^[a-fA-F0-9]{32} \*(.*)$
my $follow = 0;		# follow symbolic links

$|++;

my $ARGS = join(" ",@ARGV);
while (@ARGV) {
    $_ = shift;

    /^-(d+)/	&& do { $Debug+=length($1); next; };
    /^-f/	&& do { $MD5file = shift; next; };
    /^-w/	&& do { $warnonly++; next; };
    /^-L/	&& do { $follow++; next; };
    /^-e/	&& do { push @ExceptPattern, shift; next; };
    /^-/	&& do { warn "Unknown option $_\n$USAGE"; exit 1; };

    push @Sources, $_;
}

#print STDERR "sources: ".join("\n", @Sources)."\n"; exit;

if ($MD5file && ( -r $MD5file )) {
    # if file contains 'generated on' line, we'll use that as the 'last
    # modified' date.  otherwise we'll use the modification date of the file
    # itself.
    $^T = $^T - ( -M $MD5file )*86400;
	# examine any file with negative -M value
    my $lc = 0;
    if ( $MD5file =~ /bz2/ ) {
	$MD5file = "bzip2 -dc \"".$MD5file."\" |";
    }
    open (IN, $MD5file) or die "can't open/r $MD5file: $!\n";
    IN: while (<IN>) {
	unless ($lc) {
	    unless (/^generated/) {
		# no first-line with generated?  put one in now.
		my $time = time();
		print "generated on $time (".(localtime($time)).") $lc\n";
	    }
	}
	$lc++;
	my $fn = "";

	if (/^MD5 \((.*)\) = [a-f0-9]{32}\s*$/) {
	    $md5format=0;
	    $fn = $1;
	}
	elsif (/^[a-f0-9]{32} \*(.*)$/) {
	    $md5format=1;
	    $fn = $1;
	}
	elsif (/^generated on (\d+)\s*/) {
	    $^T = $1; # examine any file with negative -M value
	    print; next;
	}
	elsif (/^arguments: (.*)$/) {
	    warn "NOTE: earlier args were:\n\t$1\n".
		 "\tnot checking yet if these are the same\n";
	    print; next;
	}
	else {
	    warn "_unexpected input: $_";
	    print; next;
	}

	foreach my $e (@ExceptPattern) {
	    if ($fn =~ /$e/) {
		warn "x $fn (match pattern $e)\n";
		next IN;
	    }
	}

	if ((-f $fn) && ((-M $fn) > 0)) {
	    $seenfile{$1}++;
	    print;
	} else {
	    if (-f $fn) {
		warn "x $fn -M == ".(-M $fn)."\n";
	    }
	}
    }
    close (IN);
}

# make sure entries in @Sources are unique... and non-zero
{
    my %seen = ();
    foreach my $i (@Sources) { next unless ($i); $seen{$i}++; }
    @Sources = sort keys %seen;
}
my $time = time();
print STDERR ".md5 file read in ".($time - $lasttime)." sec\n"; $lasttime=$time;
print STDERR ".keys in \%seenfile: ".scalar(keys(%seenfile))."\n";
print "generated on $time (".(localtime($time))."\n";
print "arguments: $ARGS\n";
$lasttime = $time;
if ($follow) {
    find({ wanted => \&process_file, follow => 1 }, @Sources);
} else {
    find(\&process_file, @Sources);
}
$time = time();
print STDERR ".completed after ".($time - $lasttime)." sec\n";
print STDERR ".          total ".($time - $start)." sec\n";
print STDERR ".\$alreadyhad = $alreadyhad; \$md5generated = $md5generated;\n";

exit;

=cut
       The wanted() function does whatever verifications you want.
       $File::Find::dir contains the current directory name, and $_ the
       current filename within that directory.  $File::Find::name contains
       "$File::Find::dir/$_".  You are chdir()'d to $File::Find::dir when the
       function is called.  The function may set $File::Find::prune to prune
       the tree.
=cut

sub process_file {
    # directories to skip...
    if ( -d $_ )
    {
	# hard-coded...
        if (
	 ($File::Find::name =~ /c:?[\/\\]RECYCLER$/i) ||
	 (/^System Volume Information$/))
	{ $File::Find::prune++; return }
    }
    return unless ( -f $_ );

    if ($MD5file && defined($seenfile{$File::Find::name})) {
	$alreadyhad++;
	return;
    }

    foreach my $e (@ExceptPattern) {
	if ($File::Find::name =~ /$e/) {
	    warn "x $File::Find::name (match pattern $e)\n";
	    return;
	}
    }

    # compute md5 of entire file, even if we don't have music-only one
    my $ctx = Digest::MD5->new;
    unless (open (FILE, $_)) {
	my $msg = "can't open/r ".$File::Find::name.": $!\n";
	if ($warnonly) {
	    warn $msg; return;
	} else {
	    die "can't open/r ".$File::Find::name.": $!\n";
	}
    }
    binmode(FILE);
    eval {
	$ctx->addfile(*FILE);
    };
    if ($@) {
	my $msg = "fail reading ".$File::Find::name.": $@\n";
	if ($warnonly) {
	    warn $msg; return;
	} else {
	    die "can't open/r ".$File::Find::name.": $!\n";
	}
    }
    if ($md5format == 1) {
	print "".$ctx->hexdigest." *".$File::Find::name."\n";
    } else {
	print "MD5 (".$File::Find::name.") = ".$ctx->hexdigest."\n";
    }
    close (FILE);
    $md5generated++;
}
