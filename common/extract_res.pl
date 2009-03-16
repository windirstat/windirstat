#!/usr/bin/perl
use strict;
use warnings;

# Take English as the baseline
open(FILE, '<../windirstat/windirstat-utf8.rc');
my @fconts = <FILE>;
close(FILE);
my %strings;
# Read all the string IDs first of all
slurpStrings(\%strings, \@fconts);
while(my ($key, $value) = each(%strings))
{
    print "{$key}\n\t$value\n";
}

sub slurpStrings
{
    my ($refStrings, $refArray) = @_;
    my ($insidest, $currstr) = (0, undef); # inside a string table?
    foreach my $line (@{$refArray})
    {
        if($line =~ m/^STRINGTABLE/)
        {
            die "Ouch, nested string tables??? [$line]" if($insidest);
            $insidest++;
        }
        elsif(($insidest > 0) and ($line =~ m/^BEGIN/))
        {
            $insidest++; # This is the BEGIN marker of a string table
        }
        elsif(($insidest > 1) and ($line =~ m/^END/))
        {
            $insidest = 0; # We are leaving a string table
        }
        elsif($insidest > 1) # Must be a string inside a string table
        {
            if($currstr)
            {
                $currstr .= $line; # Append current line
            }
            else
            {
                $currstr = $line;
            }
            $currstr =~ s/[\n\r]//gism; # Strip any line breaks
            # Next round if we don't have a quoted string just yet
            if($currstr =~ m/"/)
            {
                # Extract the string ID and the actual string contents
                my ($stringid, $string) = $currstr =~ m/^\s*([A-Za-z_0-9]+)\s+(".+)$/;
                if($stringid and $string)
                {
                    # Now let's clean up the strings
                    $refStrings->{$stringid} = cleanupString($string);
                }
                else
                {
                    print "ERROR PARSING [$currstr]\n";
                }
                $currstr = undef;
            }
        }
    }
}

sub cleanupString
{
    my ($value) = @_;
    my ($retval) = $value =~ m/^[^"]*"(.+?)"[^"]*$/;
    $retval =~ s/\\r//gism;
    $retval =~ s/""/"/gism;
    return $retval;
}

__END__

# Go through the folder, looking for non-English resources
foreach my $folder (<../wdsr*>)
{
    next unless( -d "$folder");
    convertSingleLang($folder);
}

# Handle a single language folder
sub convertSingleLang
{
    my ($folder) = @_;
    print "$folder\n";
}
