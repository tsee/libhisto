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

print "======================================================================\n";
print " TESTING STANDALONE PERL CPAN DISTRIBUTION TARBALLS (HERMETIC BUILD)\n";
print "======================================================================\n";

# 1. Build Alien::libhisto distribution tarball
print "  [1/4] Building Alien::libhisto distribution tarball (make dist)...\n";
unlink glob(File::Spec->catfile($alien_dir, 'Alien-libhisto-*.tar.gz'));
run_cmd([$perl, 'Makefile.PL'], $alien_dir);
run_cmd([$make, 'manifest'], $alien_dir);
run_cmd([$make, 'dist'], $alien_dir);

my @alien_tars = sort glob(File::Spec->catfile($alien_dir, 'Alien-libhisto-*.tar.gz'));
die "[FAIL] No Alien-libhisto-*.tar.gz created!\n" unless @alien_tars;
my $alien_tarball = $alien_tars[-1];
print "        Generated: " . (File::Spec->splitpath($alien_tarball))[2] . "\n";

# 2. Build Math::Histo distribution tarball
print "  [2/4] Building Math::Histo distribution tarball (make dist)...\n";
unlink glob(File::Spec->catfile($math_dir, 'Math-Histo-*.tar.gz'));
run_cmd([$perl, 'Makefile.PL'], $math_dir);
run_cmd([$make, 'manifest'], $math_dir);
run_cmd([$make, 'dist'], $math_dir);

my @math_tars = sort glob(File::Spec->catfile($math_dir, 'Math-Histo-*.tar.gz'));
die "[FAIL] No Math-Histo-*.tar.gz created!\n" unless @math_tars;
my $math_tarball = $math_tars[-1];
print "        Generated: " . (File::Spec->splitpath($math_tarball))[2] . "\n";

# 3. Test extracted Alien::libhisto in isolated scratch environment
my $tmpdir = tempdir(CLEANUP => 1);
my $install_prefix = File::Spec->catdir($tmpdir, 'local_perl');
make_path($install_prefix);

print "  [3/4] Extracting & testing Alien::libhisto from tarball in $tmpdir...\n";
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

# 4. Test extracted Math::Histo against installed Alien::libhisto
print "  [4/4] Extracting & testing Math::Histo from tarball in $tmpdir...\n";
run_cmd(['tar', '-xzf', $math_tarball, '-C', $tmpdir], $repo_root);

my @extracted_maths = grep { -d $_ } glob(File::Spec->catdir($tmpdir, 'Math-Histo-*'));
die "[FAIL] Could not locate extracted Math-Histo directory in $tmpdir!\n" unless @extracted_maths;
my $extracted_math = $extracted_maths[0];

run_cmd([$perl, 'Makefile.PL', "INSTALL_BASE=$install_prefix"], $extracted_math, \%env);
run_cmd([$make], $extracted_math, \%env);
run_cmd([$make, 'test'], $extracted_math, \%env);
print "        Math::Histo built and tested successfully from tarball against installed Alien::libhisto.\n";

print "======================================================================\n";
print " RESULT: ALL PERL CPAN DISTRIBUTION TARBALL TESTS PASSED\n";
print "======================================================================\n";
exit 0;
