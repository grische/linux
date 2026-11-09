#! /usr/bin/perl -w
package avm_event_generate_struct;

use strict;
use warnings;
use input_struct;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(&struct_erzeugen %struct);
@EXPORT_OK = qw();

our %union;


##########################################################################################
# STRUCTS erzeugen
##########################################################################################
sub struct_erzeugen {
    my ( $handle, $version ) = @_;
    my $reduce = 100;
    my @bit_field_stack = ();


    foreach my $s ( sort { $struct{$a}->{priority} <=> $struct{$b}->{priority}} sort keys %struct ) {
        my $S = $struct{$s};
        my $buffer = "";
        if(defined($S->{ab_version}) and ($S->{ab_version} > $version)) {
            print STDERR "[structs] skip struct '" . $S->{name} . "' used from version " . $S->{ab_version} . ", current version " . $version . ".\n";
            next;
        }
        if(not defined($S->{priority})) {
            print "WARNING: no priority defined on struct '" . $S->{name} . "'\n";
        }
        $::defines{$S->{name}} = {
            "name" => $S->{name},
            "pos" => 10000,
            "basetype" => "struct",
            "print" => "yes",
            "depend_types" => []
        };
        $buffer = $buffer . "struct " . $S->{name} . " {\n";
        foreach my $i ( @{$S->{struct}} ) {
            if(defined($i->{ab_version}) and ($i->{ab_version} > $version)) {
                print STDERR "[struct.field] skip struct '" . $S->{name} . "." . $i->{name} . "' used from version " . $i->{ab_version} . ", current version " . $version . ".\n";
                next;
            }
            my $one_entry = "";
            if(defined($i->{bits}) and ($i->{bitoffset} == 0)) {
                $buffer = $buffer . "#if (__BYTE_ORDER == __BIG_ENDIAN)\n";
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
                $buffer = $buffer . $i->{functiontype} . ";";
            } else {
                if(defined($i->{name})) {
                    if(not defined($i->{type})) {
                        print "WARNING: no type for field " . $i->{name} . " in 'struct " . $S->{name} . "' !\n";
                        next;
                    }
                    if(defined($i->{pointer} )) {
                        if($i->{pointer} eq "yes") {
                            $one_entry = $one_entry . $i->{type} . " *" . $i->{name};
                        }
                        if($i->{pointer} eq "double") {
                            $one_entry = $one_entry . $i->{type} . " **" . $i->{name};
                        }
                    } else {
                        $one_entry = $one_entry . $i->{type} . " " . $i->{name};
                    }
                    if(defined($i->{anzahl})) {
                        $one_entry = $one_entry . "[" . $i->{anzahl} . "]";
                    }
                    if(defined($i->{bits})) {
                        $one_entry = $one_entry . " : " . $i->{bits};
                    }
                    $one_entry = $one_entry . ";";
                    if(not defined($::types{$i->{type}})) {
                        print "WARNING: type '" . $i->{type} . "' in unknown in 'struct " . $S->{name} . "' !\n";
                    }
                    if(defined($::types{$i->{type}}->{typedef}) and ($::types{$i->{type}}->{typedef} eq "yes")) {
                        if(($::types{$i->{type}}->{type} eq "struct") or ($::types{$i->{type}}->{type} eq "union")) {
                            $i->{type} = $::types{$i->{type}}->{type} . " " . $::types{$i->{type}}->{ref}->{name};
                        }
                    }
                    if(($i->{type} =~ /struct /) or ($i->{type} =~ /union /)) {
                        push @{$::defines{$S->{name}}->{depend_types}}, ( $i->{type} );
                    }
                }
            }
            if(defined($i->{comment})) {
                $one_entry = $one_entry . " /*--- " . $i->{comment} . " ---*/ ";
            }
            $one_entry = $one_entry . "\n";
            $buffer = $buffer . $one_entry;

            if(defined($i->{bits})) {
                push @bit_field_stack, ( $one_entry );
                if($i->{bits} + $i->{bitoffset} == 32) {
                    $buffer = $buffer . "#endif\n";
                    $buffer = $buffer . "#if (__BYTE_ORDER == __LITTLE_ENDIAN)\n";
                    while($#{bit_field_stack} >= 0) {
                        $buffer = $buffer . "\t" . pop @bit_field_stack;
                    }
                    $buffer = $buffer . "#endif\n";
                    @bit_field_stack = ();
                }
            }
        }
        $buffer = $buffer . "} __attribute__((packed));\n";
        if(defined($S->{typedef})) {
            $buffer = $buffer . "typedef struct " . $S->{name} . " " . $S->{typedef} . ";\n";
        }
        $buffer = $buffer . "\n";
        $::defines{$S->{name}}->{buffer} = $buffer;
        if(defined($S->{print})) {
            $::defines{$S->{name}}->{print} = $S->{print};
        }
    }
}

1;
