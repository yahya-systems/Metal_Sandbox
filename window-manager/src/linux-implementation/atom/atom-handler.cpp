
#include "atom-handler.h"
#include "../error-handler/error-handler.h"
#include "string.h"
#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

static xcb_intern_atom_cookie_t cookies[16];
static uint32_t atoms = 0;
static xcb_connection_t *xcb_atom_connection;

uint32_t submitAtoms(xcb_connection_t *xcbconnection, uint32_t atomCount,
                     const char **message, uint8_t *ifExists) {
  if (atomCount > 16) [[unlikely]]
    logError(WM_ERR_TYPE_FATAL, "CANNOT SUBMIT MORE THAN 16 ATOMS AT ONCE",
             nullptr);
  for (uint32_t i = 0; i < atomCount; ++i) {
    cookies[i] = xcb_intern_atom(xcbconnection, ifExists[i], strlen(message[i]),
                                 message[i]);
  }
  atoms = atomCount;
  xcb_atom_connection = xcbconnection;
  xcb_flush(xcb_atom_connection);
  return 0;
}

void collectAtoms(xcb_atom_t *atom) {
  if (atoms == 0)
    logError(WM_ERR_TYPE_FATAL, "DIDN'T SUBMIT ATOMS", nullptr);

  for (uint32_t i = 0; i < atoms; i++) {
    xcb_generic_error_t *error;
    xcb_intern_atom_reply_t *reply =
        xcb_intern_atom_reply(xcb_atom_connection, cookies[i], &error);
    if (reply) {
      atom[i] = reply->atom;
      free(reply);
    } else {
      if (error) {
        logErrorWithCode(WM_ERR_TYPE_FATAL, error->error_code,
                         "ATOM CREATION FAILED", error);
      }
      logError(WM_ERR_TYPE_FATAL, "ATOM CREATION FAILED FOR LOCAL REASONS",
               nullptr);
    }
  }
}
