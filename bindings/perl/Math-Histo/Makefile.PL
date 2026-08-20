use strict;
use warnings;
use ExtUtils::MakeMaker;
use File::Spec;

# In-repo development helper: add local Alien-libhisto blib to @INC if present
BEGIN {
    my $alien_blib = File::Spec->rel2abs(File::Spec->catdir(File::Spec->curdir, '..', 'Alien-libhisto', 'blib', 'lib'));
    my $alien_arch = File::Spec->rel2abs(File::Spec->catdir(File::Spec->curdir, '..', 'Alien-libhisto', 'blib', 'arch'));
    if (-d $alien_blib) {
        unshift @INC, $alien_blib, $alien_arch;
    }
}

use Alien::Base::Wrapper qw( Alien::libhisto !export );

WriteMakefile(
    Alien::Base::Wrapper->mm_args2(
        NAME          => 'Math::Histo',
        DISTNAME      => 'Math-Histo',
        VERSION_FROM  => 'lib/Math/Histo.pm',
        ABSTRACT_FROM => 'lib/Math/Histo.pm',
        AUTHOR        => 'Steffen Mueller <cpan@steffen-mueller.net>',
        LICENSE       => 'mit',
        MIN_PERL_VERSION => '5.008001',
        CONFIGURE_REQUIRES => {
            'ExtUtils::MakeMaker'  => '6.52',
            'Alien::Base::Wrapper' => '0',
            'Alien::libhisto'      => '0.1.0',
        },
        PREREQ_PM => {
            'Alien::libhisto'      => '0.1.0',
            'Exporter'             => '5.57',
            'XSLoader'             => '0',
        },
        TEST_REQUIRES => {
            'Test2::V0'            => '0.000060',
        },
        OBJECT        => '$(O_FILES)',
        META_MERGE    => {
            'meta-spec' => { version => 2 },
            resources => {
                repository => {
                    type => 'git',
                    url  => 'https://github.com/tsee/libhisto.git',
                    web  => 'https://github.com/tsee/libhisto',
                },
                bugtracker => {
                    web => 'https://github.com/tsee/libhisto/issues',
                },
            },
        },
    )
);
