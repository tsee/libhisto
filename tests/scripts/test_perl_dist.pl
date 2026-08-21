#!/usr/bin/env perl
use strict;
use warnings;
use Cwd qw(abs_path getcwd);
use File::Spec;
use File::Temp qw(tempdir);
use File::Path qw(make_path remove_tree);
use FindBin qw($Bin);
use Config;

my $make = $Config{make} || 'make';
my $perl = $^X;

sub run_cmd {
    my ($cmd, $dir, $env) = @_;
    print "        >> " . (ref($cmd) ? join(' ', @$cmd) : $cmd) . "\n";
    my $orig_dir = getcwd();
    chdir $dir if defined $dir;

    local %ENV = (%ENV, %$env) if defined $env;

    my $exit_code;
    if (ref($cmd) eq 'ARRAY') {
        $exit_code = system(@$cmd);
    } else {
        $exit_code = system($cmd);
    }

    chdir $orig_dir if defined $dir;

    if ($exit_code != 0) {
        my $status = $exit_code >> 8;
        die "\n[FAIL] Command failed with status $status in " . ($dir // $orig_dir) . "\n";
    }
}

my $repo_root = abs_path(File::Spec->catdir($Bin, '..', '..'));
my $alien_dir = File::Spec->catdir($repo_root, 'bindings', 'perl', 'Alien-libhisto');
my $math_dir  = File::Spec->catdir($repo_root, 'bindings', 'perl', 'Math-Histo');
my $pdl_dir   = File::Spec->catdir($repo_root, 'bindings', 'perl', 'Math-Histo-PDL');

print "======================================================================\n";
print " TESTING STANDALONE PERL CPAN DISTRIBUTION TARBALLS (HERMETIC BUILD)\n";
print "======================================================================\n";

# 1. Build Alien::libhisto distribution tarball
print "  [1/6] Building Alien::libhisto distribution tarball (make dist)...\n";
unlink glob(File::Spec->catfile($alien_dir, 'Alien-libhisto-*.tar.gz'));
run_cmd([$perl, 'Makefile.PL'], $alien_dir);
run_cmd([$make, 'manifest'], $alien_dir);
run_cmd([$make, 'dist'], $alien_dir);

my @alien_tars = sort glob(File::Spec->catfile($alien_dir, 'Alien-libhisto-*.tar.gz'));
die "[FAIL] No Alien-libhisto-*.tar.gz created!\n" unless @alien_tars;
my $alien_tarball = $alien_tars[-1];
print "        Generated: " . (File::Spec->splitpath($alien_tarball))[2] . "\n";

# 2. Build Math::Histo distribution tarball
print "  [2/6] Building Math::Histo distribution tarball (make dist)...\n";
unlink glob(File::Spec->catfile($math_dir, 'Math-Histo-*.tar.gz'));
run_cmd([$perl, 'Makefile.PL'], $math_dir);
run_cmd([$make, 'manifest'], $math_dir);
run_cmd([$make, 'dist'], $math_dir);

my @math_tars = sort glob(File::Spec->catfile($math_dir, 'Math-Histo-*.tar.gz'));
die "[FAIL] No Math-Histo-*.tar.gz created!\n" unless @math_tars;
my $math_tarball = $math_tars[-1];
print "        Generated: " . (File::Spec->splitpath($math_tarball))[2] . "\n";

# 3. Build Math::Histo::PDL distribution tarball
print "  [3/6] Building Math::Histo::PDL distribution tarball (make dist)...\n";
unlink glob(File::Spec->catfile($pdl_dir, 'Math-Histo-PDL-*.tar.gz'));
run_cmd([$perl, 'Makefile.PL'], $pdl_dir);
run_cmd([$make, 'manifest'], $pdl_dir);
run_cmd([$make, 'dist'], $pdl_dir);

my @pdl_tars = sort glob(File::Spec->catfile($pdl_dir, 'Math-Histo-PDL-*.tar.gz'));
die "[FAIL] No Math-Histo-PDL-*.tar.gz created!\n" unless @pdl_tars;
my $pdl_tarball = $pdl_tars[-1];
print "        Generated: " . (File::Spec->splitpath($pdl_tarball))[2] . "\n";

# 4. Test extracted Alien::libhisto in isolated scratch environment
my $tmpdir = tempdir(CLEANUP => 1);
my $install_prefix = File::Spec->catdir($tmpdir, 'local_perl');
make_path($install_prefix);

print "  [4/6] Extracting & testing Alien::libhisto from tarball in $tmpdir...\n";
run_cmd(['tar', '-xzf', $alien_tarball, '-C', $tmpdir], $repo_root);

my @extracted_aliens = grep { -d $_ } glob(File::Spec->catdir($tmpdir, 'Alien-libhisto-*'));
die "[FAIL] Could not locate extracted Alien-libhisto directory in $tmpdir!\n" unless @extracted_aliens;
my $extracted_alien = $extracted_aliens[0];

my %env = (
    PERL5LIB => File::Spec->catdir($install_prefix, 'lib', 'perl5') . ($ENV{PERL5LIB} ? ":$ENV{PERL5LIB}" : ""),
);

run_cmd([$perl, 'Makefile.PL', "INSTALL_BASE=$install_prefix"], $extracted_alien, \%env);
run_cmd([$make], $extracted_alien, \%env);
run_cmd([$make, 'test'], $extracted_alien, \%env);
run_cmd([$make, 'install'], $extracted_alien, \%env);
print "        Alien::libhisto built, tested, and installed successfully from tarball.\n";

# 5. Test extracted Math::Histo against installed Alien::libhisto
print "  [5/6] Extracting & testing Math::Histo from tarball in $tmpdir...\n";
run_cmd(['tar', '-xzf', $math_tarball, '-C', $tmpdir], $repo_root);

my @extracted_maths = grep { -d $_ } glob(File::Spec->catdir($tmpdir, 'Math-Histo-*'));
die "[FAIL] Could not locate extracted Math-Histo directory in $tmpdir!\n" unless @extracted_maths;
my $extracted_math = $extracted_maths[0];

run_cmd([$perl, 'Makefile.PL', "INSTALL_BASE=$install_prefix"], $extracted_math, \%env);
run_cmd([$make], $extracted_math, \%env);
run_cmd([$make, 'test'], $extracted_math, \%env);
run_cmd([$make, 'install'], $extracted_math, \%env);
print "        Math::Histo built, tested, and installed successfully from tarball against installed Alien::libhisto.\n";

# 6. Test extracted Math::Histo::PDL against installed Math::Histo
print "  [6/6] Extracting & testing Math::Histo::PDL from tarball in $tmpdir...\n";
run_cmd(['tar', '-xzf', $pdl_tarball, '-C', $tmpdir], $repo_root);

my @extracted_pdls = grep { -d $_ } glob(File::Spec->catdir($tmpdir, 'Math-Histo-PDL-*'));
die "[FAIL] Could not locate extracted Math-Histo-PDL directory in $tmpdir!\n" unless @extracted_pdls;
my $extracted_pdl = $extracted_pdls[0];

run_cmd([$perl, 'Makefile.PL', "INSTALL_BASE=$install_prefix"], $extracted_pdl, \%env);
run_cmd([$make], $extracted_pdl, \%env);
run_cmd([$make, 'test'], $extracted_pdl, \%env);
print "        Math::Histo::PDL built and tested successfully from tarball against installed Math::Histo.\n";

print "======================================================================\n";
print " RESULT: ALL PERL CPAN DISTRIBUTION TARBALL TESTS PASSED\n";
print "======================================================================\n";
exit 0;
