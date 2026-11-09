#! /usr/bin/perl -w

use strict;
use warnings;

use input_enum;
use input_params;
use avm_event_generate_enum;
use input_union;
use avm_event_generate_union;
use input_struct;
use avm_event_generate_struct;
use debug;

# our %types;
# our %defines;
# our @defs;
# our @includes;
# our @static_defs;



my @convert_forward = ();
my @convert_forward_defs = ();

my $enum_range;
my $enum_range_h;

##########################################################################################
# types ermitteln
##########################################################################################
  sub types_erzeugen {
      my ( $handle ) = @_;

      ##################################################################################
      # ENUM typen bekannt machen
      ##################################################################################
      foreach my $e ( sort keys %enum ) {
          my $E = $enum{$e};
          $types{"enum " . $E->{name}} = {
              "name" => "enum " . $E->{name},
              "type" => "enum",
              "size" => "sizeof(enum " . $E->{name} . ")",
              "ref" => $enum{$e}
          };
          if(defined($E->{typedef})) {
              $types{$E->{typedef}} = {
                  "name" => $E->{typedef},
                  "typedef" => "yes",
                  "type" => "enum",
                  "size" => "sizeof(" . $E->{typedef} . ")",
                  "ref" => $enum{$e}
              };
          }
      }

      ##################################################################################
      # STRUCT typen bekannt machen
      ##################################################################################
      foreach my $s ( sort keys %struct ) {
          my $S = $struct{$s};
          $types{"struct " . $S->{name}} = {
              "name" => "struct " . $S->{name},
              "type" => "struct",
              "size" => "sizeof(struct " . $S->{name} . ")",
              "ref" => $struct{$s}
          };
          if(defined($S->{typedef})) {
              $types{$S->{typedef}} = {
                  "name" => $S->{typedef},
                  "typedef" => "yes",
                  "type" => "struct",
                  "size" => "sizeof(" . $S->{typedef} . ")",
                  "ref" => $struct{$s}
              };
          }
      }

      ##################################################################################
      # UNION typen bekannt machen
      ##################################################################################
      foreach my $u ( sort keys %union ) {
          my $U = $union{$u};
          $types{"union " . $U->{name}} = {
              "name" => "union" . $U->{name},
              "type" => "union",
              "size" => undef,
              "ref" => $union{$u}
          };
          if(defined($U->{typedef})) {
              $types{$U->{typedef}} = {
                  "name" => $U->{typedef},
                  "typedef" => "yes",
                  "type" => "union",
                  "size" => undef,
                  "ref" => $union{$u}
              };
          }
      }


  }


##########################################################################################
#
##########################################################################################
sub enums_debug_erzeugen {
	my ($handle, $hfile) = @_;
    print $handle "#include \"" . $prefix . "gen_types.h\"\n\n";
	foreach my $e ( sort keys %enum ) {
		my $E = $enum{$e};
        if(defined($E->{ab_version}) and ($E->{ab_version} > $version)) {
            print STDERR "[enum-debug] skip enum '" . $E->{name} . "' used from version " . $E->{ab_version} . ", current version " . $version . ".\n";
            next;
        }
		print $hfile "char *get_enum_" . $E->{name} . "_name (enum " . $E->{name} . " value);\n";
		print $handle "char *get_enum_" . $E->{name} . "_name (enum " . $E->{name} . " value) {\n";
		print $handle "\tswitch(value) {\n";
		print $handle "\t\tdefault: return(\"" . $E->{name} . "_unknown\");\n";
		foreach my $i ( @{$E->{enum}} ) {
            if(defined($i->{ab_version}) and ($i->{ab_version} > $version)) {
                print STDERR "[enum-debug.value] skip struct '" . $E->{name} . "." . $i->{name} . "' used from version " . $i->{ab_version} . ", current version " . $version . ".\n";
                next;
            }
			if(defined($i->{name})) {
				print $handle "\t\tcase " . $i->{name} . ": return(\"" . $i->{name} . "\");\n";
			}
		}
		print $handle "\t}\n";
		print $handle "}\n";
		print $handle "\n";
	}
	print $handle "\n";
}

##########################################################################################
#  doc entry erzeugen
##########################################################################################
sub create_doc_entry {
	my ( $doc, $type ) = @_;
	my $buffer = "";
	my $indent = "";

	if(not defined($doc)) {
		return "";
	}

	if($type eq "h-file") {
		$indent = "\t";
		$buffer = "/*------------------------------------------------------------------------------------------*\\\n";
	}

	if(defined($doc->{header})) {
		$buffer = $buffer . $indent . "[ " . $doc->{header} . " ]\n\n";
	}
	if(defined($doc->{description})) {
		$buffer = $buffer . $indent . "DESCRIPTION:\n";
		$buffer = $buffer . $indent . "[ " . $doc->{description} . " ]\n\n";
	}
	if(defined($doc->{params})) {
		$buffer = $buffer . $indent . "PARAMS:\n";
		foreach my $p ( @{$doc->{params}} ) {
			$buffer = $buffer . $indent . $p->{name} . ":\t" . $p->{description} . "\n";
		}
		$buffer = $buffer . "\n";
	}
	if(defined($doc->{return})) {
		$buffer = $buffer . $indent . "RETURN:\n" . $indent . $doc->{return} . "\n";
	}

	if($type eq "h-file") {
		$buffer = $buffer .
			"/*------------------------------------------------------------------------------------------*\\\n";
	}

	return $buffer;
}

##########################################################################################
#  create endian tables
##########################################################################################
sub endian_tabelle_erzeugen {
    my ( $handle, $dot, $perl ) = @_;
    my @dots = ();
    push @convert_forward_defs, ( "#ifdef WIRESHARK_PLUGIN\n" );
    push @convert_forward_defs, ( "\tstruct enumInformation { \n" );
    push @convert_forward_defs, ( "\t\tchar *name;\n" );
    push @convert_forward_defs, ( "\t\tuint64_t value;\n" );
    push @convert_forward_defs, ( "\t};\n" );
    push @convert_forward_defs, ( "#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/\n" );

    push @convert_forward_defs, ( "struct _endian_convert { \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_IGNORE               (1)       /* wert wird ignoriert */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_LENGTH               (2)       /* dieser wert gibt die länge der gesamten struktur, incl untersturkturen an */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_SUB_LENGTH           (4)       /* dieser wert gibt die länge untersturkturen an */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_REPEAT               (8)       /* letzter wert der repeat gruppe */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_SELECT               (16)      /* selectionswert */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_SELECT_ENTRY         (32)      /* selections eintrag */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_LAST                 (64)      /* letzter Eintrag in einem struct */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_ARRAY_ELEMENT_SIZE   (128)     /* Anzahl der Elemente im array */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL (256)     /* Größe der Elemente im array */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_ARRAY_USE            (512)     /* Nutze Array größen Vorgaben */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_UNSUPPORTED          (1024)    /* unsupported */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_TOTAL_LENGTH         (2048)    /* dieser Wert gibt die Länge gesamten Struktur samt Untersturkturen an */ \n" );
    push @convert_forward_defs, ( "#define ENDIAN_CONVERT_LAST_ARRAY_ITEM (1<<31)        /* Dieses Flag markiert den letzten Eintrag in einem Array. (wird genutzt vom Lua-Protokol-Plugin) */ \n" );
    push @convert_forward_defs, ( "                                                      /* Terminieren mit einem 0-Eintrag geht nicht, da einige Array's Lücken aufweisen                  */ \n" );
    push @convert_forward_defs, ( "\tuint32_t flags; \n" );
    push @convert_forward_defs, ( "\tuint32_t offset; \n" );
    push @convert_forward_defs, ( "\tunsigned char bits; \n" );
    push @convert_forward_defs, ( "\tunsigned char bit_offset; \n" );
    push @convert_forward_defs, ( "\tuint32_t size; \n" );
    push @convert_forward_defs, ( "\tstruct _endian_convert *substruct; \n" );
    push @convert_forward_defs, ( "\tuint32_t (*enum_check_function )(uint32_t, unsigned int E); \n" );
    push @convert_forward_defs, ( "\tuint32_t ab_version;  /* 0 immer gültig */\n" );
    push @convert_forward_defs, ( "\tuint32_t bis_version;  /* 0 immer gültig */\n" );
    push @convert_forward_defs, ( "\tuint32_t default_value;  /* wird auf type des feldes gecarstet */\n" );
    push @convert_forward_defs, ( "\tchar *name; \n" );
    push @convert_forward_defs, ( "\tchar *struct_name; \n" );
    push @convert_forward_defs, ( "#ifdef WIRESHARK_PLUGIN\n" );
    push @convert_forward_defs, ( "\tstruct enumInformation *enumInfo;  // Verweis auf die generierten Enum-Hilfstabellen (Wert -> Name als String)\n" );
    push @convert_forward_defs, ( "\tchar *enumName;                    // Name der Enumeration-Tabelle: Wird fuer die Lua-Generierung benoetigt\n" );
    push @convert_forward_defs, ( "\tstruct enumInformation *enumFlags; // Verweis auf Enum-Tabellen, das das definierende Feld als Flag-Makse genutzt wird\n" );
    push @convert_forward_defs, ( "#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/\n" );
    push @convert_forward_defs, ( "}; \n\n" );

    print $handle  "#include \"" . $prefix . "gen_endian_external_defines.h\"\n\n";
    print $handle  "#include \"" . $prefix . "gen_enum_range.h\"\n\n";

    print $dot "digraph structs {\n";
    print $dot "\tcompound=true;\n";
    print $dot "\trankdir=TB;\n";
    print $dot "\tnode [shape=record];\n";

    my @perl_header_list = ( );
    my @perl_function_name_list = ( );
    my @perl_function_list = ( );

######################################################################
#
######################################################################
    foreach my $s ( sort keys %struct) {
        my $S = $struct{$s};
        my $count = 0;
        my $bits_offset = 0;
        my $last_entry = undef;
        my $bitfield_byte_offset;
        if(not defined($S->{name})) {
            next;
        }
        my $this_dot = "struct_" . $S->{name};

        my @perl_name_list = ( );
        my @perl_decode_list = ();
        my $perl_decode_list_store = 1;
        my @perl_header_decode_list = ();
        push @perl_header_decode_list, ( "\n##########################################################################################\n" );
        push @perl_header_decode_list, ( "# convert function for 'struct " . $S->{name} . "'\n" );
        push @perl_header_decode_list, ( "##########################################################################################\n" );
        push @perl_header_decode_list, ( "sub " . $prefix . "read_from_binary_struct_" . $S->{name} . " {\n" );
        push @perl_header_decode_list, ( "\tmy ( \$msg_length, \$in_ref ) = \$@;\n" );
        push @perl_header_decode_list, ( "\tmy \$offset = 0;\n" );
        push @perl_header_decode_list, ( "\tmy \$struct_offset;\n" );
        push @perl_header_decode_list, ( "\tmy \$array_element_anzahl = 0;\n" );
        push @perl_header_decode_list, ( "\tmy \$array_element_size = 1;\n" );

        push @perl_function_name_list, ( "" . $prefix . "read_from_binary_struct_" . $S->{name} );

        print $handle "\n/*------------------------------------------------------------------------------------------*\\\n";
        print $handle " * Endian Convert-Table for: 'struct " . $S->{name} . "'\n";
        print $handle "\\*------------------------------------------------------------------------------------------*/\n";
        print $handle "struct _endian_convert convert_message_struct_" . $S->{name} . "[] = {\n";
        push  @convert_forward, ("extern struct _endian_convert convert_message_struct_" . $S->{name} . "[];" );
        print $dot "\tsubgraph cluster_struct_" . $S->{name} . "{\n";
        print $dot "\t\tfontcolor = \"red\";\n";
        print $dot "\t\tlabel = \"struct " . $S->{name} . "\";\n";
        print $dot "\t\t" . $this_dot . " [fontcolor=\"green\",shape=record,label=\"{ ";
        my $seperator = "";
        my $perl_offset = 0;
        foreach my $i ( @{$S->{struct}} ) {
            $perl_decode_list_store = 1;
            if(not defined($i->{name})) {
                next;
            }
            my $ignore = "";
            if(defined($i->{functiontype})) {
                print STDERR ("WARNING: function pointer (" . $i->{name} . ") is emulated as ignored int");
                $i->{type} = "int";
                $ignore = "ENDIAN_CONVERT_IGNORE | ";
            }
            if(not defined($i->{type})) {
                print STDERR "INTERNAL Error: no type '" . $i->{name} . "' in 'struct " . $S->{name} . "'\n";
                next;
            }
            print $dot $seperator . "<" . $i->{name} . "> " . $i->{type} . ": " . $i->{name};
            if(defined($i->{selectkey})) {
                if($i->{selectkey} eq "set") {
                    print $dot "\\n(Select-Key for next union)";
                }
                if($i->{selectkey} eq "use") {
                    print $dot "\\n(Select-Key used here)";
                }
            }
            if(defined($i->{length})) {
                print $dot "\\n(length for " . $i->{length} . ")";
            }
            if(defined($i->{repeat})) {
                print $dot "\\n(repeat repeat" . $i->{repeat} . ")";
            }
            $seperator = "|";
            print $handle "\t[" . $count . "] = {\n";
            print $handle "\t\t.struct_name =  \"convert_message_struct_" . $S->{name} . "\",\n";
            if(defined($i->{ab_version})) {
                printf $handle "\t\t.ab_version  = 0x%X,\n", $i->{ab_version};
            }
            if(defined($i->{bis_version})) {
                print $handle "\t\t.bis_version = " . $i->{bis_version} . ",\n";
            }
            if(defined($i->{default})) {
                print $handle "\t\t.default_value = (uint32_t)(" . $i->{default} . "),\n";
            }
            print $handle "\t\t.flags = ";
            my $last = @{$S->{struct}}[scalar(@{$S->{struct}})-1];
            print $handle $ignore;
            if($i == $last) {
                print $handle "ENDIAN_CONVERT_LAST | ";
            }
            if(defined($i->{selectkey})) {
                if($i->{selectkey} eq "set") {
                    print $handle "ENDIAN_CONVERT_SELECT | ";
                }
                if($i->{selectkey} eq "use") {
                    print $handle "ENDIAN_CONVERT_SELECT_ENTRY | ";
                }
            }
            if(defined($i->{array})) {
                if($i->{array} eq "element_anzahl") {
                    print $handle "ENDIAN_CONVERT_ARRAY_ELEMENT_ANZAHL | ";
                }
                if($i->{array} eq "element_size") {
                    print $handle "ENDIAN_CONVERT_ARRAY_ELEMENT_SIZE | ";
                }
                if($i->{array} eq "use") {
                    print $handle "ENDIAN_CONVERT_ARRAY_USE | ";
                }
            }
            if((defined $i->{type}) && (defined $types{$i->{type}}->{type}) && ($types{$i->{type}}->{type} eq "union")) {
                print $handle "ENDIAN_CONVERT_IGNORE | ";
            }
            if(defined($i->{repeat})) {
                print $handle "ENDIAN_CONVERT_REPEAT | ";
            }
            if(defined($i->{length})) {
                if($i->{length} eq "sublength") {
                    print $handle "ENDIAN_CONVERT_SUB_LENGTH | ";
                } elsif($i->{length} eq "total") {
                    print $handle "ENDIAN_CONVERT_TOTAL_LENGTH | ";
                }
              }

            # Kennzeichnet das letzte Element eines Arrays
            # das ist nötig, da die Array's Lücken haben und das Wireshark-Protokoll-Plugin das
            # ganze nicht mehr konvertieren kann.
            if($i == $last || defined($i->{isLastEndianElement})) {
                print $handle "ENDIAN_CONVERT_LAST_ARRAY_ITEM | ";
            }

            print $handle "0,\n";
            print $handle "\t\t.name = \"" . $i->{name} . "\",\n";
            if(defined($i->{bits})) {
                print $handle "\t\t.bits = " . $i->{bits} . ",\n";
                if($i->{bitoffset} == 0) {
                    if(defined($last_entry)) {
                        $bitfield_byte_offset = "__builtin_offsetof(struct " . $S->{name} . ", " . $last_entry->{name} . ")";
                    } else {
                        $bitfield_byte_offset = "0";
                    }
                }
                if($bits_offset != $i->{bitoffset}) {
                    print STDERR "Internal definition error on bitfield on: " . $S->{name} . " -> " . $i->{name} . "\n";
                    print STDERR "\tinternal bits_offset: " . $bits_offset . " definition bitoffset " . $i->{bitoffset} . "\n";
                    exit -1;
                }
                $bits_offset += $i->{bits};
                if($bits_offset == 32) {
                    $bits_offset = 0;
                }
                print $handle "\t\t.bit_offset = " . $i->{bitoffset} . ",\n";
            }
#######################################################
# type prüfen
#######################################################
            if(not defined($types{$i->{type}})) {
                print STDERR "WARNING: type '" . $i->{type} . " unknown\n";
            }
            if(defined($types{$i->{type}}->{typedef}) and ($types{$i->{type}}->{typedef} eq "yes")) {
                if(($types{$i->{type}}->{type} eq "struct") or ($types{$i->{type}}->{type} eq "union")) {
#                    print "RRRRRRRRRRRRRRRRR" . " replace type " . $i->{type} . " by ";
                    $i->{type} = $types{$i->{type}}->{type} . " " . $types{$i->{type}}->{ref}->{name};
#                    print $i->{type} . "\n";
                }
            }

            if ( (defined $i->{type}) && (!defined $types{$i->{type}}->{type})){
                print STDERR "WARNING: no type defined in types->{$i->{type}}\n";
                $types{$i->{type}}->{type}='';
            }

# print "name=" . $i->{name} . " type=" . $i->{type} . "\n";
            push @perl_name_list, ( { "type" => "\$", "name" => $i->{name}, "init" => "" } );
            my $perl_size_found = 0;
            my $perl_array_loop = 0;
            if(($types{$i->{type}}->{type} eq "native") or ($types{$i->{type}}->{type} eq "enum")) {
                my $size = 0;
                my $perl_size = 0;
                my $perl_decode = "";

                if(defined($i->{array})) {
                    if($i->{array} eq "use") {
                        pop @perl_name_list;
                        push @perl_name_list, ( { "type" => "\@", "name" => $i->{name}, "init" => " = ()" } );
                        $perl_decode = $perl_decode . "\n\t\$for ( my \$i = 0 ; \$i < \$array_element_anzahl ; \$i++ ) {\n";
                        $perl_decode = $perl_decode . "\n\t\tmy \@unpack_type = ( \" \", \"C\", \">S\", \">C>S\", \">I\" );\n";
                        $perl_decode = $perl_decode . "\n\t\t\$" . $i->{name} . "[\$i] = unpack(\"a\" . \$offset . \"/\" . \$unpack_type[\$array_element_size], \$in_ref );\n";
                        $perl_decode = $perl_decode . "\t}\n";
                        if ( $perl_decode_list_store eq 1 ) {
                            $perl_decode_list_store = 0;
                            push @perl_decode_list, ( $perl_decode );
                        }
                    }
                } else {
                    if(defined($i->{anzahl})) {
                        if($i->{anzahl} eq 0) {
                            pop @perl_name_list;
                            push @perl_name_list, ( { "type" => "\@", "name" => $i->{name}, "init" => " = ()" } );
                            $perl_decode = $perl_decode . "\n\twhile ( \$offset < \$msg_length ) {\n";
                            $perl_array_loop = 1;
                            $i->{anzahl} = $i->{size};
                        }
                    }
                }
                if(defined($i->{bits})) {
                    $perl_decode =  $perl_decode . "\n\t# component '" . $i->{name} . "' hat '" . $i->{bits} . "' bits at offset '" .
                        $i->{bitoffset} . "' bits\n";
                } else {
                    if($perl_array_loop == 1) {
                        $perl_decode =  $perl_decode . "\n\t\t";
                    } else {
                        $perl_decode =  $perl_decode . "\n\t";
                    }
                    $perl_decode =  $perl_decode . "# c-struct component '" . $i->{name} . "' at offset '" . $perl_offset . "'\n";
                }

                if($perl_array_loop == 1) {
                    $perl_decode =  $perl_decode . "\t\tpush \@" . $i->{name} . ", ( unpack(\"";
                } else {
                    $perl_decode =  $perl_decode . "\t\$" . $i->{name} . " = unpack(\"";
                }
                if(($perl_offset gt 0) or ( $perl_array_loop == 1)) {
                    $perl_decode =  $perl_decode . "a\" . \$offset . \"/";
                }

                ##########################################################################################
                # BIT TYPES
                ##########################################################################################
                if(defined($i->{bits})) {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_decode = $perl_decode . "\", \$in_ref);\n";
                    $perl_decode = $perl_decode . "\t\$" . $i->{name} . " = ( \$" . $i->{name} . " >> " . $i->{bitoffset} . " ) & ((1 << " . $i->{bits} . ") - 1);\n";


                    if($i->{bitoffset} + $i->{bits} >= 32) {
                        $perl_offset += 4;
                        $perl_decode = $perl_decode . "\t\$offset += 4; # end of bitfield\n";
                    }
                    if(defined($i->{array})) {
                        if($i->{array} eq "element_anzahl") {
                            $perl_decode = $perl_decode . "\t\$array_element_anzahl  = \$" . $i->{name} . ";\n";
                        }
                        if($i->{array} eq "element_size") {
                            $perl_decode = $perl_decode . "\t\$array_element_size = \$" . $i->{name} . ";\n";
                        }
                    }
                    if($perl_array_loop == 1) {
                            $perl_decode = $perl_decode . "\t}\n";
                    }
                    if ( $perl_decode_list_store eq 1 ) {
                        $perl_decode_list_store = 0;
                        push @perl_decode_list, ( $perl_decode );
                    }
                }


                ##########################################################################################
                # BYTE TYPES
                ##########################################################################################
                if(defined($types{$i->{type}}->{size})) {
                    $size = $types{$i->{type}}->{size};
                }
                if(defined($i->{pointer}) and ($i->{pointer} ne "no")) {
                    $size = "sizeof(" . $i->{type} . " *)";
                }
                if($size eq "sizeof(char)") {
                    $perl_decode = $perl_decode .  "C";  # bei 'C' gibt es kein bin und little endian
                    $perl_offset += 1;
                    $perl_size = 1;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(unsigned char)") {
                    $perl_decode = $perl_decode .  "C";  # bei 'C' gibt es kein bin und little endian
                    $perl_offset += 1;
                    $perl_size = 1;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(int8_t)") {
                    $perl_decode = $perl_decode .  ">S";
                    $perl_offset += 1;
                    $perl_size = 1;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint8_t)") {
                    $perl_decode = $perl_decode .  ">S";
                    $perl_offset += 1;
                    $perl_size = 1;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(int16_t)") {
                    $perl_decode = $perl_decode .  ">S";
                    $perl_offset += 2;
                    $perl_size = 2;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint16_t)") {
                    $perl_decode = $perl_decode .  ">S";
                    $perl_offset += 2;
                    $perl_size = 2;
                    $perl_size_found = 1;
                }
                if($size =~ /sizeof\(.*\*\)/) {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(int32_t)") {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint32_t)") {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint32_t)") {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(int32_t)") {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint32_t)") {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if($size eq "sizeof(uint64_t)") {
                    $perl_decode = $perl_decode .  ">L";
                    $perl_offset += 8;
                    $perl_size = 8;
                    $perl_size_found = 1;
                }
                if(($size =~ /sizeof\(enum/) or ($types{$i->{type}}->{type} eq "enum")) {
                    $perl_decode = $perl_decode .  ">I";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }

                if($size eq "sizeof(double)") {
                    $perl_decode = $perl_decode .  ">d";
                    $perl_offset += 4;
                    $perl_size = 4;
                    $perl_size_found = 1;
                }
                if(defined($i->{anzahl})) {
                    my $anzahl = $i->{anzahl};
                    foreach my $zahl ( @defs ) {
                        if($zahl->{name} eq $anzahl) {
                            $anzahl = $zahl->{value};
                            last;
                        }
                    }
                    if ($anzahl=~/[^\d\s]/){
                        print STDERR qq{WARNING: cannot resolve offset from "$anzahl"}."\n";
                    }else{
                        $perl_offset += ($anzahl - 1) * $perl_size;
                    };
                    if( $anzahl gt 0 ) {
                        $size = $size . " * " . $anzahl;
                    } else {
                        $size = $size;
                    }
                    $perl_decode = $perl_decode .  $anzahl;
                    $perl_size   = $anzahl;
                }
                print $handle "\t\t.size = " . $size . ",\n";
                if($perl_size_found eq 0) {
                    pwarn("type = " . $i->{type} . ", size = " . $size . " ->type-> " . $types{$i->{type}}->{type});
                }
                if($perl_array_loop == 1) {
                    $perl_decode = $perl_decode . "\", \$in_ref) ) ;\n";
                } else {
                    $perl_decode = $perl_decode . "\", \$in_ref);\n";
                }
                if($perl_array_loop == 1) {
                    $perl_decode = $perl_decode . "\t";
                }
                $perl_decode = $perl_decode . "\t\$offset += " . $perl_size . ";\n";
                if(defined($i->{array})) {
                    if($i->{array} eq "element_anzahl") {
                        $perl_decode = $perl_decode . "\t\$array_element_anzahl  = \$" . $i->{name} . ";\n";
                    }
                    if($i->{array} eq "element_size") {
                        $perl_decode = $perl_decode . "\t\$array_element_size = \$" . $i->{name} . ";\n";
                    }
                }
                if($perl_array_loop == 1) {
                        $perl_decode = $perl_decode . "\t}\n";
                }
                if ( $perl_decode_list_store eq 1 ) {
                    $perl_decode_list_store = 0;
                    push @perl_decode_list, ( $perl_decode );
                }
            }
            if($perl_size_found eq 0) {
                ##########################################################################################
                # STRUCT TYPES
                ##########################################################################################
                my $perl_name = $i->{type};
                my $perl_decode = "";
                $perl_name =~ s/ /_/g;
                if($perl_array_loop == 1) {
                    $perl_decode =  $perl_decode . "\n\t\t";
                } else {
                    $perl_decode =  $perl_decode . "\n\t";
                }
                $perl_decode =  $perl_decode . "# c-struct component '" . $i->{name} . "' at offset '" . $perl_offset . "'\n";
                $perl_decode =  $perl_decode . "\t( \$" . $i->{name} . ", \$struct_offset ) = " . $prefix . "read_from_binary_" . $perl_name . "(\n\t\t\t\t( \$msg_length - \$offset), unpack(\"";
                if($perl_offset gt 0) {
                    $perl_decode = $perl_decode . "a\" . \$offset . \"/";
                }
                $perl_decode = $perl_decode . "C\" . (\$msg_length - \$offset), \$in_ref));\n";
                $perl_decode = $perl_decode . "\t\$offset += \$struct_offset;\n";

                if ( $perl_decode_list_store eq 1 ) {
                    $perl_decode_list_store = 0;
                    push @perl_decode_list, ( $perl_decode );
                }
            }

            if($types{$i->{type}}->{type} eq "struct") {
                my $that_dot = $i->{type};
                my $substruct = "convert_message_" . $i->{type};
                $substruct =~ s/ /_/g;
                $that_dot =~ s/ /_/g;
                print $handle "\t\t.substruct = " . $substruct . ",\n";
                push @convert_forward, ("extern struct _endian_convert " . $substruct . "[];" );
                push @dots, ( "\t" . $this_dot . ":" . $i->{name} . " -> " . $that_dot . " [lhead=cluster_" . $that_dot . "];");
            }
            if($types{$i->{type}}->{type} eq "union") {
                my $that_dot = $i->{type};
                my $substruct = $i->{type};
                $substruct =~ s/ /_/g;
                $that_dot =~ s/ /_/g;
                print $handle "\t\t.enum_check_function = (uint32_t (*)(uint32_t,uint32_t))" .
                        "check_enum_for_" . $substruct . ",\n";
                $substruct = "convert_message_" . $substruct;
                print $handle "\t\t.substruct = " . $substruct . ",\n";
                push @convert_forward, ("extern struct _endian_convert " . $substruct . "[];" );
                push @dots, ( "\t" . $this_dot . ":" . $i->{name} . " -> " . $that_dot . " [lhead=cluster_" . $that_dot . "];");
            }
            if(defined($i->{bits})) {
                print $handle "\t\t.offset = " .  $bitfield_byte_offset . ",\n";
            } else {
                print $handle "\t\t.offset = __builtin_offsetof(struct " . $S->{name} . ", " . $i->{name} . "),\n";
            }

            # Den verwendeten Enum-Typen des Feldes mit generieren.
            # Sinn: Das Wireshark-Protokoll-Plugin kann dann den String-Namen nutzen
            #       und anzeigen.
            if($i->{type} =~ m/enum .+/) {
                $i->{type} =~ m/enum (.+)/;
                my $enumName = $1;

                print $handle "#ifdef WIRESHARK_PLUGIN\n";
                # bei manchen Strukturen sind die Enums geshifted, und da man das in dem Wireshark-Plugin nicht
                # auseinanderfummeln kann (es sind feste Feld-Objekte in denen man nicht definieren kann, dass
                # er doch bitte geshifted auf die Text-Tabelle zugreifen soll), deshalb die neue Tabelle hier
                if(defined($i->{is_shifted})) {
                  print $handle "\t\t.enumInfo = enum_shifted_table_" . $enumName . ",\n";
                  print $handle "\t\t.enumName = \"enum_shifted_table_" . $enumName . "\"\n";
                } else {
                  print $handle "\t\t.enumInfo = enum_table_" . $enumName . ",\n";
                  print $handle "\t\t.enumName = \"enum_table_" . $enumName . "\"\n";
                }
                print $handle "#else /*--- #ifdef WIRESHARK_PLUGIN ---*/\n";

                # check_enum_range Funktionen generieren, aber nur wenn keine Enum-Maske
                print $handle "\t\t.enum_check_function = ";
                if(defined($i->{selectkey}) && $i->{selectkey} eq "set") {
                  print $handle "(uint32_t (*)(uint32_t,uint32_t))" . $prefix . "check_enum_range_" . $enumName;
                } else {
                  print $handle "NULL // enum field, but possibly a mask, so not check function";
                }
                print $handle ",\n";

                print $handle "#endif /*--- #else ---*/ /*--- #ifdef WIRESHARK_PLUGIN ---*/\n";
            } elsif(defined($i->{enumFlags})) {
              # besitzt eine Definition dieses Feld, beinhaltet es Bit-Flags
              # -> das Wireshark-Plugin kann dieses dann aufdröseln
                print $handle "#ifdef WIRESHARK_PLUGIN\n";
                print $handle "\t\t.enumFlags = enum_table_" . $i->{enumFlags} . ",\n";
                print $handle "#endif /*--- #ifdef WIRESHARK_PLUGIN ---*/\n";
            }

            print $handle "\t},\n";
            $count++;
            $last_entry = $i;
        }
        # Structs können auch leer sein
        # -> NULL-Eintrag mit ENDIAN_CONVERT_LAST_ARRAY_ITEM erzeugen
        # ganze nicht mehr konvertieren kann.
        if(0 == scalar(@{$S->{struct}})) {
            print $handle "\t\t[0] = {\n";
            print $handle "\t\t\t.flags = ENDIAN_CONVERT_LAST_ARRAY_ITEM | ENDIAN_CONVERT_LAST\n";
            print $handle "\t\t}\n";
        }

        print $dot "}\"];\n";
        print $dot "\t};\n";
        print $handle "};\n";

        push @perl_function_list, @perl_header_decode_list;

        push @perl_function_list, ( "\n" );
        push @perl_function_list, ( "\t####### liste of hash components #######\n" );
        foreach my $n ( @perl_name_list ) {
            push @perl_function_list, ( "\tmy " . $n->{type} . $n->{name} . $n->{init} . ";\n" );
        }
        push @perl_function_list, ( "\n\t####### convert binary message to hash components #######\n" );
        push @perl_function_list, ( join('', @perl_decode_list) . "\n\n" );

        push @perl_function_list, ( "\t####### return hash and used length #######\n" );
        push @perl_function_list, ( "\t\$return ( {\n" );
        foreach my $n ( @perl_name_list ) {
            if($n->{type} eq "\$") {
                push @perl_function_list, ( "\t\t\"" . $n->{name} . "\" => \$" . $n->{name} . ",\n" );
            } else { if($n->{type} eq "\@") {
                push @perl_function_list, ( "\t\t\"" . $n->{name} . "\" => [ \@" . $n->{name} . " ],\n" );
                }
            }
        }
        push @perl_function_list, ( "\t}, \$offset );\n" );
        push @perl_function_list, ( "}\n\n" );
    }

    print $perl "#! /usr/bin/perl -w\n";
    print $perl "package " . $prefix . "perl_convert;\n";
    print $perl "use strict;\n";
    print $perl "use warnings;\n";
    print $perl "use Exporter ();\n\n";
    print $perl "our (\$VERSION, \@ISA, \@EXPORT, \@EXPORT_OK, %EXPORT_TAGS);\n\n";
    print $perl "\$VERSION     = 1.00;\n";

    print $perl "\@ISA = qw(Exporter);\n";
    print $perl "\@EXPORT = qw(" . join("\n\t\t&", @perl_function_name_list ) . "\n\t);\n\n";
    print $perl "\@EXPORT_OK = qw();\n\n";

    print $perl join('', @perl_function_list);

    print $perl "\n\n1;\n";


######################################################################
#
######################################################################
    foreach my $u ( sort keys %union ) {
        my $U = $union{$u};
        my $count = 0;
        my $bits_offset = 0;
        my $this_dot = "union_" . $U->{name};
        print $handle "\n/*------------------------------------------------------------------------------------------*\\\n";
        print $handle " * Endian Convert-Table for: 'union " . $U->{name} . "'\n";
        print $handle "\\*------------------------------------------------------------------------------------------*/\n";
        print $handle "struct _endian_convert convert_message_union_" . $U->{name} . "[] = {\n";
        push  @convert_forward, ("extern struct _endian_convert convert_message_struct_" . $U->{name} . "[];" );
        print $dot "\tsubgraph cluster_union_" . $U->{name} . "{\n";
        print $dot "\t\tfontcolor = \"red\";\n";
        print $dot "\t\tlabel = \"union " . $U->{name} . "\";\n";
        print $dot "\t\t" . $this_dot . " [shape=record,fontcolor=\"blue\",label=\"{ ";
        my $seperator = "";

        if(not defined($U->{selecttype})) {
            print STDERR "WARNING: Selecttype not defined on union " . $U->{name} . "\n" ;
            $U->{selecttype} = "int";
        }
        print $enum_range_h
            "extern int check_enum_for_union_" . $U->{name} . " (" . $U->{selecttype} . " E, unsigned int version __attribute__((unused)));\n";

        print $enum_range
            "\n/*------------------------------------------------------------------------------------------*\\\n" .
            " * Endian check für : 'union " . $U->{name} . "'\n" .
            "\\*------------------------------------------------------------------------------------------*/\n" .
            "int check_enum_for_union_" . $U->{name} . " (" . $U->{selecttype} . " E, unsigned int version __attribute__((unused))) {\n" .
            "\tswitch(E) {\n";

        foreach my $i ( @{$U->{union}} ) {
            if(not defined($i->{name})) {
                next;
            }
            print $dot $seperator . "<" . $i->{name} . "> " . $i->{type} . ": " . $i->{name};
            $seperator = "|";
#			if(defined($i->{select})) {
#				print $handle "\t\t.select_count = " . ($#{$i->{select}} + 1) . "\n";
#				print $handle "\t\t.select = { ";
#                foreach my $s ( @{$i->{select}} ) {
#                    print $handle $s . ", ";
#                }
#                print $handle "},\n";
#			}
            my $select = $count;
            my $select_count_max = 0;
            my $select_count;
            if(defined($i->{select})) {
                $select_count_max = $#{$i->{select}};
            }
            for($select_count = 0 ; $select_count <= $select_count_max ; $select_count++) {
                if(defined($i->{select})) {
                    $select = ${$i->{select}}[$select_count]
                }
                print $handle "\t[" . $select . "] = {\n";
                print $handle "\t\t.struct_name =  \"convert_message_union_" . $U->{name} . "\",\n";
                print $enum_range "\t\tcase " . $select . ":\n";
                if(defined($i->{ab_version})) {
                    printf $handle "\t\t.ab_version  = 0x%X,\n", $i->{ab_version};
                    print $enum_range "\t\t\tif(version < " . $i->{ab_version} . ")\n";
                    print $enum_range "\t\t\t\treturn 1;\n";
                }
                print $enum_range "\t\t\treturn 0;\n";
                print $handle "\t\t.flags = ";
                print $handle "ENDIAN_CONVERT_LAST | ";
                if(defined($i->{repeat})) {
                    print $handle "ENDIAN_CONVERT_REPEAT | ";
                }

                # Kennzeichnet das letzte Element eines Arrays
                # das ist nötig, da die Array's Lücken haben und das Wireshark-Protokoll-Plugin das
                # ganze nicht mehr konvertieren kann.
                my $last = @{$U->{union}}[scalar(@{$U->{union}})-1];
                if($i == $last) {
                    print $handle "ENDIAN_CONVERT_LAST_ARRAY_ITEM | ";
                }

                print $handle "0,\n";

                # Es gibt mehrere select-Einträge.
                # Generiert haben die alle denselben Namen -> schlecht (speziell für die Wireshark-Protokoll-Plugin-Generierung)
                if($select_count_max > 1) {
                    print $handle "\t\t.name = \"" . $i->{name} . "_" . $select . "\",\n";
                } else {
                    print $handle "\t\t.name = \"" . $i->{name} . "\",\n";
                }

                if(defined($i->{bits})) {
                    print $handle "\t\t.bits = " . $i->{bits} . ",\n";
                    $bits_offset += $i->{bits};
                    if($bits_offset == 32) {
                        $bits_offset = 0;
                    }
                    print $handle "\t\t.bit_offset = " . $bits_offset . ",\n";
                }
#######################################################
# type prüfen
#######################################################
                if(not defined($types{$i->{type}})) {
                    print STDERR "WARNING: type '" . $i->{type} . " unknown\n";
                }
                if(defined($types{$i->{type}}->{typedef}) and ($types{$i->{type}}->{typedef} eq "yes")) {
                    if(($types{$i->{type}}->{type} eq "struct") or ($types{$i->{type}}->{type} eq "union")) {
#                    print "RRRRRRRRRRRRRRRRR" . " replace type " . $i->{type} . " by ";
                        $i->{type} = $types{$i->{type}}->{type} . " " . $types{$i->{type}}->{ref}->{name};
#                    print $i->{type} . "\n";
                    }
                }
# print "name=" . $i->{name} . " type=" . $i->{type} . "\n";
                if(($types{$i->{type}}->{type} eq "native") or ($types{$i->{type}}->{type} eq "enum")) {
                    my $size = 0;
                    if(defined($types{$i->{type}}->{size})) {
                        $size = $types{$i->{type}}->{size};
                    }
                    if(defined($i->{pointer}) and ($i->{pointer} ne "no")) {
                        $size = "sizeof(" . $i->{type} . " *)";
                    }
                    if(defined($i->{anzahl})) {
                        $size = $size . " * " . $i->{anzahl};
                    }
                    print $handle "\t\t.size = " . $size . ",\n";
                }
                if($types{$i->{type}}->{type} eq "struct") {
                    my $that_dot = $i->{type};
                    my $substruct = "convert_message_" . $i->{type};
                    $substruct =~ s/ /_/g;
                    $that_dot =~ s/ /_/g;
                    push @convert_forward, ("extern struct _endian_convert " . $substruct . "[];" );
                    print $handle "\t\t.substruct = " . $substruct . ",\n";
                    if($select_count == 0) {
                        if(defined($i->{select})) {
                            push @dots, ( "\t" . $this_dot . ":" . $i->{name} . " -> " . $that_dot . " [lhead=cluster_" . $that_dot .
                                    ",label=\"select=\\n'" .  join("\\n", @{$i->{select}}) . "'\"];");
                        } else {
                            push @dots, ( "\t" . $this_dot . ":" . $i->{name} . " -> " . $that_dot . " [lhead=cluster_" . $that_dot . "];");
                        }
                    }
                }
                if($types{$i->{type}}->{type} eq "union") {
                    my $that_dot = $i->{type};
                    my $substruct = "convert_message_" . $i->{type};
                    $substruct =~ s/ /_/g;
                    $that_dot =~ s/ /_/g;
                    push @convert_forward, ("extern struct _endian_convert " . $substruct . "[];" );
                    print $handle "\t\t.substruct = " . $substruct . ",\n";
                    if($select_count == 0) {
                        push @dots, ( "\t" . $this_dot . ":" . $i->{name} . " -> " . $that_dot . " [lhead=cluster_" . $that_dot . "];");
                    }
                }
                print $handle "\t\t.offset = __builtin_offsetof(union " . $U->{name} . ", " . $i->{name} . ")\n";
                print $handle "\t},\n";
                $count++;
            }

        }
        print $dot "}\"];\n";
        print $dot "\t};\n";
        print $handle "};\n";

        print $enum_range "\t\tdefault:\n";
        print $enum_range "\t\t\treturn 1;\n";
        print $enum_range "\t}\n";
        print $enum_range "}\n\n";

    }
    print $dot join("\n", @dots);
    print $dot "\n};\n";

}


##########################################################################################
# Printing localtime to debug resolving priority will change the build's fingerprint (e.g. md5).
# For this reason we use a single counter here.
# To print the current version into into the build, git commit could be used, as it is stable.
##########################################################################################
sub prioritaet_erzeugen {
    my ($reduce) = @_;
    my $changes = "no";

    my $pos_update_time=0;

	foreach my $e_key ( sort keys %defines ) {
        my $e = $defines{$e_key};
        $e->{pos} = 0 unless defined $e->{pos};
        my $this_prio = $e->{pos};
        if(not defined($e->{name}) or (length($e->{name}) < 1)) {
            print STDERR "skip one entry\n";
            next;
        }
        foreach my $d ( @{$e->{depend_types}} ) {
            $d =~ s/union //g;
            $d =~ s/struct //g;
            $defines{$d}->{pos} = 0 unless defined $defines{$d}->{pos};
            my $depend_prio = $defines{$d}->{pos};
#            print "depend_prio='" . $depend_prio . "\n";
#            print "this_prio='" . $this_prio . "\n";
            if($this_prio <= $depend_prio) {
#            print "this_prio='" . $this_prio . "' >= ";
#            print "depend_prio='" . $depend_prio . "' (reduce by " . $reduce . ")\n";
                $defines{$d}->{pos} = $this_prio - $reduce;
                $defines{$d}->{pos_update_time} = $pos_update_time++;
                $changes = "yes";
            }
        }
    }
    return $changes;
};

##########################################################################################
#  MAIN MAIN
##########################################################################################
sub main {
    pdebug("-------- type composer Version 1.0, Oktober 2012 --------");
    my $hfile;
    input_params_init();
    input_enum_init();
    input_struct_init();
    input_union_init();

    open $enum_range, ">" . "" . $prefix . "gen_enum_range.c";
    open $enum_range_h, ">" . "" . $prefix . "gen_enum_range.h";

    open $hfile, ">" . "" . $prefix . "gen_types.h";

    print $hfile "#ifndef _" . $prefix . "gen_types_h_\n";
    print $hfile "#define _" . $prefix . "gen_types_h_\n";
    print $hfile "\n";
    foreach my $f (@includes) {
        if(defined($f->{internal})) {
            if($f->{internal} eq "yes") {
                print $hfile "#ifdef	" . uc($prefix) . "_INTERNAL\n";
            }
            if($f->{internal} eq "no") {
                print $hfile "#ifndef	" . uc($prefix) . "_INTERNAL\n";
            }
        }
        if(defined($f->{ifdef})) {
            print $hfile "#ifdef	" . $f->{ifdef} ."\n";
        }
        if(defined($f->{ifndef})) {
            print $hfile "#ifndef	" . $f->{ifndef} ."\n";
        }
        if(defined($f->{kernel})) {
            if($f->{kernel} eq "yes") {
                print $hfile "#ifdef	__KERNEL__\n";
            }
            if($f->{kernel} eq "no") {
                print $hfile "#ifndef	__KERNEL__\n";
            }
        }
        if($f->{local} eq "yes") {
            print $hfile "#include \"" . $f->{file} . "\"\n";
        }
        if($f->{local} eq "no") {
            print $hfile "#include <" . $f->{file} . ">\n";
        }
        if(defined($f->{kernel})) {
            print $hfile "#endif\n";
        }
        if(defined($f->{internal})) {
            print $hfile "#endif\n";
        }
        if(defined($f->{ifdef})) {
            print $hfile "#endif\n";
        }
        if(defined($f->{ifndef})) {
            print $hfile "#endif\n";
        }
    }
    print $hfile "\n";
    foreach my $d (@defs) {
        print $hfile "#define " . $d->{name} . " " . $d->{value} . "\n";
    }
    print $hfile "\n";
    foreach my $sd (@static_defs) {
        if(defined($sd->{kernel})) {
            if($sd->{kernel} eq "yes") {
                print $hfile "#ifdef   __KERNEL__\n";
            }
            if($sd->{kernel} eq "no") {
                print $hfile "#ifndef  __KERNEL__\n";
            }
        }
        if(defined($sd->{internal})) {
            if($sd->{internal} eq "yes") {
                print $hfile "#ifdef   " . uc($prefix) . "_INTERNAL\n";
            }
            if($sd->{internal} eq "no") {
                print $hfile "#ifndef  " . uc($prefix) . "_INTERNAL\n";
            }
        }
        print $hfile $sd->{defs} . "\n";
        if(defined($sd->{internal})) {
            print $hfile "#endif\n";
        }
        if(defined($sd->{kernel})) {
            print $hfile "#endif\n";
        }
    }
    print $hfile "\n";

    types_erzeugen($hfile, $version);
    enums_erzeugen($hfile, $version);
    struct_erzeugen($hfile, $version);
    union_erzeugen($hfile, $version);

    my $debug;
    open $debug, ">" . "" . $prefix . "gen_debug.c";
    enums_debug_erzeugen($debug, $hfile);
    close $debug;

    enums_range_erzeugen($enum_range, $enum_range_h);

    my $reduce = 100;
    for ( ;; ) {
        if(prioritaet_erzeugen($reduce) eq "no") {
            last;
        }
        $reduce = $reduce / 2;
    }

    # prevent warnings by setting undefined values to zero as they will be handled so anyway
    for my $key (sort keys %defines){
        $defines{$key}->{pos} = 0 if (defined $defines{$key}) && (! defined $defines{$key}->{pos});
    }

    foreach my $line ( sort { $defines{$a}->{pos} <=> $defines{$b}->{pos} } sort keys %defines ) {
        my $define = $defines{$line};
        next unless $define;
        next if (defined $define->{print}) && ($define->{print} eq "no");

        if(defined($define->{pos_update_time})) {
            print $hfile "/* pos " . $define->{pos} . " (" . $define->{pos_update_time} . ") */\n";
        }

        if (defined $define->{buffer}){
            print $hfile $define->{buffer}
        }
    }

    print $hfile "\n#endif /*--- #ifndef _" . $prefix . "gen_types_h_ ---*/\n";
    close $hfile;

    my $endian;
    my $perl;
    my $dot;
    open $dot, ">" . "" . $prefix . "gen_flow.dot";
    open $perl, ">" . "" . $prefix . "perl_convert.pm";
    open $endian, ">" . "" . $prefix . "gen_endian.h";
    endian_tabelle_erzeugen ( $endian, $dot, $perl );

    my $ext;
    open $ext, ">" . "" . $prefix . "gen_endian_external_defines.h";

    my $last_funcdef = "";
    print $ext "#ifndef _AVM_EVENT_GENERATE_EXTERN_DEFS_H_\n";
    print $ext "#define _AVM_EVENT_GENERATE_EXTERN_DEFS_H_\n";

    print $ext join("", @convert_forward_defs) . "\n";
    foreach my $funcdef ( sort @convert_forward ) {
        if($funcdef eq $last_funcdef) {
            next;
        }
        $last_funcdef = $funcdef;
        print $ext $funcdef . "\n";
    }

    print $ext "#endif /*--- #ifndef _AVM_EVENT_GENERATE_EXTERN_DEFS_H_ ---*/\n";
    close $ext;
    close $endian;
    close $dot;
    close $enum_range;
    close $enum_range_h;
}


main();
