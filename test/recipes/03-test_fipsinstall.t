#! /usr/bin/env perl
# Copyright 2019-2023 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

use strict;
use warnings;

use File::Spec::Functions qw(:DEFAULT abs2rel);
use File::Copy;
use OpenSSL::Glob;
use OpenSSL::Test qw/:DEFAULT srctop_dir srctop_file bldtop_dir bldtop_file/;
use OpenSSL::Test::Utils;

BEGIN {
    setup("test_fipsinstall");
}
use lib srctop_dir('Configurations');
use lib bldtop_dir('.');
use platform;

plan skip_all => "Test only supported in a fips build" if disabled("fips");

# Compatible options for pedantic FIPS compliance
my @pedantic_okay =
    ( 'ems_check', 'no_drbg_truncated_digests', 'self_test_onload',
      'kmac_length_checks'
    );

# Incompatible options for pedantic FIPS compliance
my @pedantic_fail =
    ( 'no_conditional_errors', 'no_security_checks', 'self_test_oninstall' );

plan tests => 38 + (scalar @pedantic_okay) + (scalar @pedantic_fail);

my $infile = bldtop_file('providers', platform->dso('fips'));
my $fipskey = $ENV{FIPSKEY} // config('FIPSKEY') // '00';
my $provconf = srctop_file("test", "fips-and-base.cnf");

# Read in a text $infile and replace the regular expression in $srch with the
# value in $repl and output to a new file $outfile.
sub replace_line_file_internal {

    my ($infile, $srch, $repl, $outfile) = @_;
    my $msg;

    open(my $in, "<", $infile) or return 0;
    read($in, $msg, 1024);
    close $in;

    $msg =~ s/$srch/$repl/;

    open(my $fh, ">", $outfile) or return 0;
    print $fh $msg;
    close $fh;
    return 1;
}

# Read in the text input file 'fips.cnf'
# and replace a single Key = Value line with a new value in $value.
# OR remove the Key = Value line if the passed in $value is empty.
# and then output a new file $outfile.
# $key is the Key to find
sub replace_line_file {
    my ($key, $value, $outfile) = @_;

    my $srch = qr/$key\s*=\s*\S*\n/;
    my $rep;
    if ($value eq "") {
        $rep = "";
    } else {
       $rep = "$key = $value\n";
    }
    return replace_line_file_internal('fips.cnf', $srch, $rep, $outfile);
}

# Read in the text input file 'test/fips.cnf'
# and replace the .cnf file used in
# .include fipsmodule.cnf with a new value in $value.
# and then output a new file $outfile.
# $key is the Key to find
sub replace_parent_line_file {
    my ($value, $outfile) = @_;
    my $srch = qr/fipsmodule.cnf/;
    my $rep = "$value";
    return replace_line_file_internal(srctop_file("test", 'fips.cnf'),
                                      $srch, $rep, $outfile);
}

# Check if the specified pattern occurs in the given file
# Returns 1 if the pattern is found and 0 if not
sub find_line_file {
    my ($key, $file) = @_;

    open(my $in, $file) or return -1;
    while (my $line = <$in>) {
        if ($line =~ /$key/) {
            close($in);
            return 1;
        }
    }
    close($in);
    return 0;
}

# fail if no module name
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module',
             '-provider_name', 'fips',
             '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
             '-section_name', 'fips_sect'])),
   "fipsinstall fail");

# fail to verify if the configuration file is missing
ok(!run(app(['openssl', 'fipsinstall', '-in', 'dummy.tmp', '-module', $infile,
             '-provider_name', 'fips', '-mac_name', 'HMAC',
             '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
             '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail");


# output a fips.cnf file containing mac data
ok(run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect'])),
   "fipsinstall");

# verify the fips.cnf file
ok(run(app(['openssl', 'fipsinstall', '-in', 'fips.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify");

ok(replace_line_file('module-mac', '', 'fips_no_module_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-in', 'fips_no_module_mac.cnf',
                '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
                '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail no module mac");

ok(replace_line_file('install-mac', '', 'fips_no_install_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-in', 'fips_no_install_mac.cnf',
                '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
                '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail no install indicator mac");

ok(replace_line_file('module-mac', '00:00:00:00:00:00',
                     'fips_bad_module_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-in', 'fips_bad_module_mac.cnf',
                '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
                '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail if invalid module integrity value");

ok(replace_line_file('install-mac', '00:00:00:00:00:00',
                     'fips_bad_install_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-in', 'fips_bad_install_mac.cnf',
                '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
                '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail if invalid install indicator integrity value");

ok(replace_line_file('install-status', 'INCORRECT_STATUS_STRING',
                     'fips_bad_indicator.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-in', 'fips_bad_indicator.cnf',
                '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
                '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail if invalid install indicator status");

# fail to verify the fips.cnf file if a different key is used
ok(!run(app(['openssl', 'fipsinstall', '-in', 'fips.cnf', '-module', $infile,
             '-provider_name', 'fips', '-mac_name', 'HMAC',
             '-macopt', 'digest:SHA256', '-macopt', "hexkey:01",
             '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail bad key");

# fail to verify the fips.cnf file if a different mac digest is used
ok(!run(app(['openssl', 'fipsinstall', '-in', 'fips.cnf', '-module', $infile,
             '-provider_name', 'fips', '-mac_name', 'HMAC',
             '-macopt', 'digest:SHA512', '-macopt', "hexkey:$fipskey",
             '-section_name', 'fips_sect', '-verify'])),
   "fipsinstall verify fail incorrect digest");

# corrupt the module hmac
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'HMAC'])),
   "fipsinstall fails when the module integrity is corrupted");

# corrupt the first digest
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'SHA1'])),
   "fipsinstall fails when the digest result is corrupted");

# corrupt another digest
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'SHA3'])),
   "fipsinstall fails when the digest result is corrupted");

# corrupt cipher encrypt test
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'AES_GCM'])),
   "fipsinstall fails when the AES_GCM result is corrupted");

# corrupt cipher decrypt test
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'AES_ECB_Decrypt'])),
   "fipsinstall fails when the AES_ECB result is corrupted");

# corrupt DRBG
ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf', '-module', $infile,
            '-provider_name', 'fips', '-mac_name', 'HMAC',
            '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
            '-section_name', 'fips_sect', '-corrupt_desc', 'CTR'])),
   "fipsinstall fails when the DRBG CTR result is corrupted");

# corrupt a KAS test
SKIP: {
    skip "Skipping KAS DH corruption test because of no dh in this build", 1
        if disabled("dh");

    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
                '-section_name', 'fips_sect',
                '-corrupt_desc', 'DH',
                '-corrupt_type', 'KAT_KA'])),
       "fipsinstall fails when the kas result is corrupted");
}

# corrupt a Signature test - 140-3 requires a known answer test
SKIP: {
    skip "Skipping Signature DSA corruption test because of no dsa in this build", 1
        if disabled("dsa");

    run(test(["fips_version_test", "-config", $provconf, ">=3.1.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version is too old for KAT DSA signature test", 1
        if !$exit;
    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
                '-section_name', 'fips_sect', '-self_test_oninstall',
                '-corrupt_desc', 'DSA',
                '-corrupt_type', 'KAT_Signature'])),
       "fipsinstall fails when the signature result is corrupted");
}

# corrupt a Signature test - 140-2 allows a pairwise consistency test
SKIP: {
    skip "Skipping Signature DSA corruption test because of no dsa in this build", 1
        if disabled("dsa");

    run(test(["fips_version_test", "-config", $provconf, "<3.1.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version is too new for PCT DSA signature test", 1
        if !$exit;
    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
                '-section_name', 'fips_sect',
                '-corrupt_desc', 'DSA',
                '-corrupt_type', 'PCT_Signature'])),
       "fipsinstall fails when the signature result is corrupted");
}

# corrupt an Asymmetric cipher test
SKIP: {
    skip "Skipping Asymmetric RSA corruption test because of no rsa in this build", 1
        if disabled("rsa");
    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
                '-corrupt_desc', 'RSA_Encrypt',
                '-corrupt_type', 'KAT_AsymmetricCipher'])),
       "fipsinstall fails when the asymmetric cipher result is corrupted");
}

# 'local' ensures that this change is only done in this file.
local $ENV{OPENSSL_CONF_INCLUDE} = abs2rel(curdir());

ok(replace_parent_line_file('fips.cnf', 'fips_parent.cnf')
   && run(app(['openssl', 'fipsinstall', '-config', 'fips_parent.cnf'])),
   "verify fips provider loads from a configuration file");

ok(replace_parent_line_file('fips_no_module_mac.cnf',
                            'fips_parent_no_module_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-config', 'fips_parent_no_module_mac.cnf'])),
   "verify load config fail no module mac");

SKIP: {
    run(test(["fips_version_test", "-config", $provconf, "<3.1.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version doesn't support self test indicator", 3
        if !$exit;

    ok(replace_parent_line_file('fips_no_install_mac.cnf',
                                'fips_parent_no_install_mac.cnf')
       && !run(app(['openssl', 'fipsinstall',
                    '-config', 'fips_parent_no_install_mac.cnf'])),
       "verify load config fail no install mac");

    ok(replace_parent_line_file('fips_bad_indicator.cnf',
                                'fips_parent_bad_indicator.cnf')
       && !run(app(['openssl', 'fipsinstall',
                    '-config', 'fips_parent_bad_indicator.cnf'])),
       "verify load config fail bad indicator");


    ok(replace_parent_line_file('fips_bad_install_mac.cnf',
                                'fips_parent_bad_install_mac.cnf')
       && !run(app(['openssl', 'fipsinstall',
                    '-config', 'fips_parent_bad_install_mac.cnf'])),
       "verify load config fail bad install mac");
}

ok(replace_parent_line_file('fips_bad_module_mac.cnf',
                            'fips_parent_bad_module_mac.cnf')
   && !run(app(['openssl', 'fipsinstall',
                '-config', 'fips_parent_bad_module_mac.cnf'])),
   "verify load config fail bad module mac");

SKIP: {
    run(test(["fips_version_test", "-config", $provconf, "<3.1.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version doesn't support self test indicator", 3
        if !$exit;

    my $stconf = "fipsmodule_selftest.cnf";

    ok(run(app(['openssl', 'fipsinstall', '-out', $stconf,
                '-module', $infile, '-self_test_onload'])),
           "fipsinstall config saved without self test indicator");

    ok(!run(app(['openssl', 'fipsinstall', '-in', $stconf,
                 '-module', $infile, '-verify'])),
            "fipsinstall config verify fails without self test indicator");

    ok(run(app(['openssl', 'fipsinstall', '-in', $stconf,
                '-module', $infile, '-self_test_onload', '-verify'])),
           "fipsinstall config verify passes when self test indicator is not present");
}

SKIP: {
    run(test(["fips_version_test", "-config", $provconf, ">=3.1.0"]),
             capture => 1, statusvar => \my $exit);
    skip "FIPS provider version can run self tests on install", 1
        if !$exit;
    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
                '-provider_name', 'fips', '-mac_name', 'HMAC',
                '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
                '-section_name', 'fips_sect', '-self_test_oninstall',
                '-ems_check'])),
       "fipsinstall fails when attempting to run self tests on install");
}

ok(find_line_file('drbg-no-trunc-md = 0', 'fips.cnf') == 1,
   'fipsinstall defaults to not banning truncated digests with DRBGs');

ok(run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
           '-provider_name', 'fips', '-mac_name', 'HMAC',
           '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
           '-section_name', 'fips_sect', '-no_drbg_truncated_digests'])),
   "fipsinstall knows about allowing truncated digests in DRBGs");

ok(find_line_file('drbg-no-trunc-md = 1', 'fips.cnf') == 1,
   'fipsinstall will allow option for truncated digests with DRBGs');

ok(find_line_file('kmac-length-checks = 0', 'fips.cnf') == 1,
   'fipsinstall defaults to not banning truncated digests with DRBGs');

ok(run(app(['openssl', 'fipsinstall', '-out', 'fips.cnf', '-module', $infile,
           '-provider_name', 'fips', '-mac_name', 'HMAC',
           '-macopt', 'digest:SHA256', '-macopt', "hexkey:$fipskey",
           '-section_name', 'fips_sect', '-kmac_length_checks'])),
   "fipsinstall knows about enforcing KMAC length checks");

ok(find_line_file('kmac-length-checks = 1', 'fips.cnf') == 1,
   'fipsinstall will allow option for KMAC length checks');

ok(run(app(['openssl', 'fipsinstall', '-out', 'fips-pedantic.cnf',
            '-module', $infile, '-pedantic'])),
       "fipsinstall accepts -pedantic option");

foreach my $o (@pedantic_okay) {
    ok(run(app(['openssl', 'fipsinstall', '-out', "fips-${o}.cnf",
                '-module', $infile, '-pedantic', "-${o}"])),
           "fipsinstall accepts -${o} after -pedantic option");
}

foreach my $o (@pedantic_fail) {
    ok(!run(app(['openssl', 'fipsinstall', '-out', 'fips_fail.cnf',
                 '-module', $infile, '-pedantic', "-${o}"])),
            "fipsinstall disallows -${o} after -pedantic option");
}

