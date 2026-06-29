package My::Suite::Disabled_functions;

@ISA = qw(My::Suite);

return "No DISABLED_FUNCTIONS plugin" unless $ENV{DISABLED_FUNCTIONS_SO};

return "Not run for embedded server" if $::opt_embedded_server;

push @::global_suppressions,
  (
    qr/Plugin 'disabled_functions' is of maturity level experimental while the server is alpha/,
  );

sub is_default { 1 }

bless { };
