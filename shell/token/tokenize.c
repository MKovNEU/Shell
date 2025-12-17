/**
 * Tokenizer demo program.
 */
#include <stdio.h>

#include <assert.h>

#include "vect.h"
#include "tokens.h"

const int BUFFER_SIZE = 1024;

int main(int argc, char **argv) {
  char buffer[BUFFER_SIZE];

  if (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
    /* Tokenize the input from stdin... */  
    vect_t *tokens = tokenize(buffer);

    assert(tokens != NULL);

    /* ...and print each token on a separate line */
    for (size_t i = 0; i < vect_size(tokens); i++) {
      printf("%s\n", vect_get(tokens, i));
    }

    vect_delete(tokens);
  }

}
