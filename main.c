#include <ruby.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
  ruby_init();
  // ruby_init_loadpath();

  VALUE result = rb_eval_string("puts 'Hello, world!'");

  return ruby_cleanup(0);
}