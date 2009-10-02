package Smokeping::probes::RtlPing;

=head1 301 Moved Permanently

This is a Smokeping probe module. Please use the command

C<smokeping -man Smokeping::probes::RtlPing>

to view the documentation or the command

C<smokeping -makepod Smokeping::probes::RtlPing>

to generate the POD document.

=cut

use strict;
use base qw(Smokeping::probes::basefork);
use IPC::Open3;
use Symbol;
use Carp;

sub pod_hash {
	return {
		name => <<DOC,
Smokeping::probes::RtlPing - RtlPing probe for Smokeping
DOC
		description => <<DOC,
Integrates RtlPing of OpenRRCP project as a probe into smokeping.

RRCP(Realtek Remote Configuration Protocol) is protocol for making some
specific low-cost dumb ethernet switches act like more expensive managed
switches with no or little hardware modifications.

OpenRRCP is an open-source cross-platform RRCP-based toolset, that is able to
configure and fetch status from such ethernet switches.

The variable B<binary> must point to your copy of the rtlping program.
If it is not installed on your system yet, you you can get it from
L<http://openrrcp.org.ru/>
rtlping must be installed setuid root or it will not work
host must be in '[[authkey-]xx:xx:xx:xx:xx:xx@]if-name' format
DOC
		authors => <<'DOC',
Alexey Illarionov <littlesavage@rambler.ru>
DOC
		see_also => <<DOC
DOC
	};
}

sub new($$$)
{
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self = $class->SUPER::new(@_);

    return $self;
}

sub probevars {
	my $class = shift;
	return $class->_makevars($class->SUPER::probevars, {
		_mandatory => [ 'binary' ],
		binary => {
			_doc => "The location of your rtlping binary.",
			_example => '/usr/bin/rtlping',
			_sub => sub {
				my $val = shift;
        			return "ERROR: rtlping 'binary' does not point to an executable"
            				unless -f $val and -x _;
				return undef;
			},
		},
		hostinterval => {
			_doc => "Wait between sending each packet, 0.1 - 2.0 seconds",
			_example => 0.2,
			_default => 1.0,
			_sub => sub {
				my $val = shift;
				return "ERROR: wait option must be between 0.1 and 2.0"
					unless $val + 0.0 >= 0.1 and $val + 0.0 <= 2.0;
				return undef;
			}
		}
	});
}

sub targetvars {
	my $class = shift;
	return $class->_makevars($class->SUPER::targetvars, {
	      pingmode => {
		 _doc => "Ping mode: rrcp, rep, or rrcrep",
		 _default => 'rrcp',
		 _re => '(rrcp)|(RRCP)|(rep)|(REP)|(rrcprep)|(RRCPREP)'
	      }
	});
}

sub ProbeDesc($){
    my $self = shift;
    my $mode = uc($self->{properties}{pingmode} || 'RRCP');
    return "RRCP pings, $mode mode";
}

sub pingone ($){
    my $self = shift;
    my $target = shift;

    my $binary = $self->{properties}{binary};

    # ping one target
    my @cmd = ($self->{properties}{binary},
       '-q',
       '-c', scalar $self->pings($target));

   push (@cmd, '-m', $target->{vars}{pingmode})
      if ($target->{vars}{pingmode});
   push (@cmd, '-i', int($self->{properties}{hostinterval} * 1000))
      if ($self->{properties}{hostinterval});
   push (@cmd, $target->{addr});
   push (@cmd, '2>&1');

   my @times;
   my $inh = gensym;
   my $outh = gensym;
   my $errh = gensym;
    $self->do_debug("Executing @cmd");
    my $pid = open3($inh,$outh,$errh, @cmd);
    while (<$outh>){
        chomp;
	$self->do_debug("Got rtlping output: '$_'");
        next unless /^\S+\s+:\s+[-\d\.]/; #filter out error messages from rtlping
        my @str = split /\s+/;
        my $addr = shift @str;
        next unless ':' eq shift @str; #drop the colon
        @times = map {sprintf "%.10e", $_ / 1000.0} sort {$a <=> $b} grep /^\d/, @str;
    }
    waitpid $pid,0;
    close $inh;
    close $outh;
    close $errh;

    return @times;
}

1;
