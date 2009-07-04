#!/usr/bin/perl
use strict;
use warnings;

# Take English as the baseline
open(FILE, '<../windirstat/windirstat-utf8.rc');
my @fconts = <FILE>;
close(FILE);
my %strings;
# Read all the string IDs first of all
#slurpStrings(\%strings, \@fconts);
#slurpMenus(\%strings, \@fconts);
slurpControls(\%strings, \@fconts);

while(my ($key, $value) = each(%strings))
{
    print "{$key}\n\t$value\n";
}

# Parse strings (STRINGTABLE and childs) from RC scripts
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

# Parse menus (MENUITEM + POPUP) from RC scripts
sub slurpMenus
{
    my ($refStrings, $refArray) = @_;
    my ($insidemn, $currstr, $cnt) = (0, undef, 0); # inside a menu?
    foreach my $line (@{$refArray})
    {
        if($line =~ m/^\s*?([A-Za-z0-9_]+)\s+MENU/)
        {
            die "Ouch, nested menus??? [$line]" if($insidemn);
            $insidemn++;
        }
        elsif(($insidemn > 0) and ($line =~ m/^\s*?BEGIN/))
        {
            $insidemn++; # This is the BEGIN marker of a menu
        }
        elsif(($insidemn > 1) and ($line =~ m/^\s*?END/))
        {
            $insidemn--; # We are leaving a menu
            if($insidemn == 1)
            {
                $insidemn--;
            }
        }
        elsif($insidemn > 1) # Must be something inside a menu
        {
            $currstr = $line;
            $currstr =~ s/[\n\r]//gism; # Strip any line breaks
            # Next round if we don't have a quoted string just yet
            # Extract the ID and the actual string contents
            my ($stringid, $string) = (undef, undef);
            unless($currstr =~ /^\s*?MENUITEM\s+?SEPARATOR/)
            {
                # Parse POPUP item
                ($stringid, $string) = $currstr =~ m/^\s*(POPUP)\s+(".+)$/;
                # Not a POPUP item?
                unless($stringid and $string)
                {
                    # Parse MENUITEM item
                    ($stringid, $string) = $currstr =~ m/^\s*(MENUITEM)\s+(".+)$/;
                    if($stringid and $string)
                    {
                        ($string, $stringid) = $string =~ m/^(".+?),\s*([A-Za-z0-9_]+)\s*$/;
                        $stringid = "MENUITEM_" . $stringid;
                    }
                }
                else
                {
                    $stringid = "POPUP_" . $stringid . sprintf("%03d", $cnt++);
                }
                if($stringid and $string)
                {
                    # Now let's clean up the strings
                    $refStrings->{$stringid} = cleanupString($string);
                }
                else
                {
                    print "ERROR PARSING [$currstr]\n";
                }
            }
        }
    }
}

# Parse dialogs (DIALOGEX and child controls) from RC scripts
sub slurpControls
{
    my ($refStrings, $refArray) = @_;
    my ($insidectl, $currstr, $cnt) = (0, undef, 0); # inside a menu?
    foreach my $line (@{$refArray})
    {
        if($line =~ m/^\s*?([A-Za-z0-9_]+)\s+DIALOGEX?/)
        {
            $currstr = $1; # Use the ID
            die "Ouch, nested dialogs??? [$line]" if($insidectl);
            $insidectl++;
        }
        elsif(($insidectl > 0) and ($line =~ m/^\s*?CAPTION/))
        {
            $insidectl++; # This is the BEGIN marker of a menu
        }
        elsif(($insidectl > 0) and ($line =~ m/^\s*?BEGIN/))
        {
            $insidectl++; # This is the BEGIN marker of a menu
        }
        elsif(($insidectl > 1) and ($line =~ m/^\s*?END/))
        {
            $insidectl--; # We are leaving a menu
            if($insidectl == 1)
            {
                $insidectl--;
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


        elsif($insidectl > 1) # Must be something inside a menu
        {
            $currstr = $line;
            $currstr =~ s/[\n\r]//gism; # Strip any line breaks
            # Next round if we don't have a quoted string just yet
            # Extract the ID and the actual string contents
            my ($stringid, $string) = (undef, undef);
            unless($currstr =~ /^\s*?MENUITEM\s+?SEPARATOR/)
            {
                # Parse POPUP item
                ($stringid, $string) = $currstr =~ m/^\s*(POPUP)\s+(".+)$/;
                # Not a POPUP item?
                unless($stringid and $string)
                {
                    # Parse MENUITEM item
                    ($stringid, $string) = $currstr =~ m/^\s*(MENUITEM)\s+(".+)$/;
                    if($stringid and $string)
                    {
                        ($string, $stringid) = $string =~ m/^(".+?),\s*([A-Za-z0-9_]+)\s*$/;
                        $stringid = "MENUITEM_" . $stringid;
                    }
                }
                else
                {
                    $stringid = "POPUP_" . $stringid . sprintf("%03d", $cnt++);
                }
                if($stringid and $string)
                {
                    # Now let's clean up the strings
                    $refStrings->{$stringid} = cleanupString($string);
                }
                else
                {
                    print "ERROR PARSING [$currstr]\n";
                }
            }
        }
