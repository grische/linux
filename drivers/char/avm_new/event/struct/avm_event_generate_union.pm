#! /usr/bin/perl -w
package avm_event_generate_union;

use strict;
use warnings;
use input_union;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(&union_erzeugen);
@EXPORT_OK = qw();

our %union;


##########################################################################################
# UNIONS erzeugen
##########################################################################################
sub union_erzeugen {
	my ( $handle, $version ) = @_;
	foreach my $u ( sort keys %union ) {
		my $U = $union{$u};
        my $buffer = "";
        if(defined($U->{ab_version}) and ($U->{ab_version} > $version)) {
            next;
        }
        $::defines{$U->{name}} = {
            "name" => $U->{name},
            "pos" => 10000,
            "basetype" => "union",
            "print" => "yes",
            "depend_types" => []
        };
		if(defined($U->{selecttype})) {
			$buffer = $buffer . "/*-----------------------------------------------------------------------------------\n";
			$buffer = $buffer . "\tunion entry is select by variable of type '" . $U->{selecttype} . "'\n";
			$buffer = $buffer . "-----------------------------------------------------------------------------------*/\n";
		}
		$buffer = $buffer . "union " . $U->{name} . " {\n";
		foreach my $i ( @{$U->{union}} ) {
            if(defined($i->{ab_version}) and ($i->{ab_version} > $version)) {
                next;
            }
			if(defined($i->{select})) {
				$buffer = $buffer . "\t/*--- select by [select-variable] one of (" . (join" ", @{$i->{select}}) . ") ---*/\n";
			}
			$buffer = $buffer . "\t";
			if(defined($i->{group_comment})) {
				$buffer = $buffer . "/*--- " . $i->{group_comment} . " ---*/ ";
			}
            if(defined($i->{ab_version})) {
                $buffer = sprintf("%s/*--- Valid since version 0x%X ---*/\n", $buffer, $i->{ab_version});
			    $buffer = $buffer . "\t";
            }
			if(defined($i->{functiontype})) {
				$buffer = $buffer . ";";
			} else {
				if(defined($i->{name})) {
					if(defined($i->{pointer} ) and ($i->{pointer} eq "yes")) {
						$buffer = $buffer . $i->{type} . " *" . $i->{name};
					} else {
						$buffer = $buffer . $i->{type} . " " . $i->{name};
					}
					if(defined($i->{anzahl})) {
						$buffer = $buffer . "[" . $i->{anzahl} . "]";
					}
					if(defined($i->{bits})) {
						$buffer = $buffer . " : " . $i->{bits};
					}
					$buffer = $buffer . ";";
					if(not defined($::types{$i->{type}})) {
						print "WARNING: type '" . $i->{type} . "' in unknown in 'union " . $U->{name} . "' !\n";
					}
                    if(defined($::types{$i->{type}}->{typedef}) and ($::types{$i->{type}}->{typedef} eq "yes")) {
                        if(($::types{$i->{type}}->{type} eq "struct") or ($::types{$i->{type}}->{type} eq "union")) {
                            $i->{type} = $::types{$i->{type}}->{type} . " " . $::types{$i->{type}}->{ref}->{name};
                        }
                    }
                    if(($i->{type} =~ /struct /) or ($i->{type} =~ /union /)) {
                        push @{$::defines{$U->{name}}->{depend_types}}, ( $i->{type} );
                    }
				}
			}
			if(defined($i->{comment})) {
				$buffer = $buffer . " /*--- " . $i->{comment} . " ---*/ ";
			}
			$buffer = $buffer . "\n";
		}
		$buffer = $buffer . "};\n";
		if(defined($U->{typedef})) {
			$buffer = $buffer . "typedef union " . $U->{name} . " " . $U->{typedef} . ";\n";
		}
		$buffer = $buffer . "\n";
        $::defines{$U->{name}}->{buffer} = $buffer;
        if(defined($U->{print})) {
            $::defines{$U->{name}}->{print} = $U->{print};
        }
	}
}

1;
