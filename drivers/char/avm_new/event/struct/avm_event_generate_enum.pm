#! /usr/bin/perl -w
package avm_event_generate_enum;

use strict;
use warnings;
use input_enum;
use input_params;

use Exporter ();
our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
$VERSION     = 1.00;

@ISA = qw(Exporter);
@EXPORT = qw(&enums_erzeugen &enums_range_erzeugen);
@EXPORT_OK = qw();

our %enum;


##########################################################################################
# ENUMS RANGE erzeugen
##########################################################################################
sub enums_range_erzeugen {
    my ( $handle, $hfile ) = @_;
    print $hfile "#ifndef _" . $prefix . "gen_enum_range_h_\n";
    print $hfile "#define _" . $prefix . "gen_enum_range_h_\n\n";
    print $hfile "#include \"" . $prefix . "gen_types.h\"\n\n";
    print $handle "#include \"" . $prefix . "gen_enum_range.h\"\n";
    foreach my $e ( sort keys %enum ) {
        my $E = $enum{$e};
        if(not defined($E->{name}) or (length($E->{name}) < 1)) {
            print "INTERNAL ERROR: enum with no name\n";
            next;
        }

        print $hfile "extern uint32_t " . $prefix . "check_enum_range_" . $E->{name} . "(enum " . $E->{name} . " E);\n";
        print $handle "\nuint32_t " . $prefix . "check_enum_range_" . $E->{name} . "(enum " . $E->{name} . " E) {\n";
        print $handle "\tswitch(E) {\n";
        foreach my $i ( @{$E->{enum}} ) {
            if(defined($i->{name}) && defined($i->{value})) {
                print $handle "\t\tcase " . $i->{name} . ":\n";
            }
        }
        print $handle "\t\t\treturn 0;\n";
        print $handle "\t\tdefault:\n";
        print $handle "\t\t\treturn 1;\n";
        print $handle "\t}\n";
        print $handle "}\n";
    }
    print $hfile "\n#endif /*--- #ifndef _" . $prefix . "gen_enum_range_h_ ---*/\n";
}

##########################################################################################
# ENUMS erzeugen
##########################################################################################
sub enums_erzeugen {
	my ( $handle, $version ) = @_;
	foreach my $e ( sort keys %enum ) {
		my $E = $enum{$e};
        if(defined($E->{ab_version}) and ($E->{ab_version} > $version)) {
            print STDERR "[enum] skip enum '" . $E->{name} . "' used from version " . $E->{ab_version} . ", current version " . $version . ".\n";
            next;
        }
        if(not defined($E->{name}) or (length($E->{name}) < 1)) {
            print "INTERNAL ERROR: enum with no name\n";
            next;
        }

		print $handle "enum " . $E->{name} . " {\n";
		foreach my $i ( @{$E->{enum}} ) {
            if(defined($i->{ab_version}) and ($i->{ab_version} > $version)) {
                print STDERR "[enum.value] skip struct '" . $E->{name} . "." . $i->{name} . "' used from version " . $i->{ab_version} . ", current version " . $version . ".\n";
                next;
            }
			print $handle "\t";
			if(defined($i->{group_comment})) {
				print $handle "/*--- " . $i->{group_comment} . " ---*/ ";
			}
            if(defined($i->{ab_version})) {
                printf $handle "/*--- Valid since version 0x%X ---*/\n", $i->{ab_version};
			    print $handle "\t";
            }
			if(defined($i->{name})) {
				print $handle $i->{name};
				if(defined($i->{value})) {
					print $handle " = " . $i->{value};
				}

				print $handle ",";
				if(defined($i->{comment})) {
					print $handle " /*--- " . $i->{comment} . " ---*/ ";
				}
			}
			print $handle "\n";
		}
		foreach my $i ( @{$E->{alias}} ) {
			print $handle "\t";
			if(defined($i->{group_comment})) {
				print $handle "/*--- " . $i->{group_comment} . " ---*/ ";
			}
			if(defined($i->{name})) {
				print $handle $i->{name};
				if(defined($i->{value})) {
					print $handle " = " . $i->{value};
				}

				print $handle ",";
				if(defined($i->{comment})) {
					print $handle " /*--- " . $i->{comment} . " ---*/ ";
				}
			}
			print $handle "\n";
		}
		print $handle "};\n";
		if(defined($E->{typedef})) {
			print $handle "typedef enum " . $E->{name} . " " . $E->{typedef} . ";\n";
		}
		print $handle "\n";

      # Die enum so in eine Tabelle generieren, dass das Wireshark-Protokoll-Plugin das Zeug
      # gut verarbeiten kann.
      print $handle "#ifdef WIRESHARK_PLUGIN\n";
      if(defined($E->{prefix})) {
        print $handle "/*--- Hinweis: der prefix '" . $E->{prefix} . "' wird durch das wireshark plugin entfernt ---*/\n";
      }
      print $handle "struct enumInformation enum_table_" . $E->{name} . "[] = {\n";
      foreach my $i ( @{$E->{enum}} ) {
          if(!defined($i->{name})) {
              # das sind group comments
              next;
          }
          my $name = $i->{name};
          if(defined($E->{prefix})) {
              $name =~ s/$E->{prefix}/$prefix/g;
          }

          print $handle "\t{ .name = \"" . $name . "\", .value = ";
          if(!defined($i->{value})) {
              print $handle $i->{name};
          } else {
              print $handle $i->{value};
          }
          print $handle " },\n";
      }
      print $handle "\t{0} // last element\n";
      print $handle "};\n";

      # bei manchen Strukturen sind die Enums geshifted, und da man das in dem Wireshark-Plugin nicht
      # auseinanderfummeln kann (es sind feste Feld-Objekte in denen man nicht definieren kann, dass
      # er doch bitte geshifted auf die Text-Tabelle zugreifen soll), deshalb die neue Tabelle hier
      if(defined($E->{wireshark_gen_shiftedTable})) {
        print $handle "struct enumInformation enum_shifted_table_" . $E->{name} . "[] = {\n";
        foreach my $i ( @{$E->{enum}} ) {
          if(!defined($i->{name})) {
            # das sind group comments
            next;
          }
          my $name = $i->{name};
          if(defined($E->{prefix})) {
            $name =~ s/$E->{prefix}/$prefix/g;
          }

          print $handle "\t{ .name = \"" . $name . "\", .value = (uint64_t)1 << ";
          if(!defined($i->{value})) {
            print $handle $i->{name};
          } else {
            print $handle $i->{value};
          }
          print $handle " },\n";
        }
        print $handle "\t{0} // last element\n";
        print $handle "};\n";
      }
      print $handle "#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/\n";
      print $handle "\n";
  }
	print $handle "\n";
}

1;
