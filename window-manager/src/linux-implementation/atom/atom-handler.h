
#ifndef WM_ATOM_HANDLER
#define WM_ATOM_HANDLER
#include <cstdint>
#include <xcb/xcb.h>
#include <xcb/xcb_atom.h>

uint32_t submitAtoms(xcb_connection_t *connection, uint32_t atomCount,
                     const char **message, uint8_t *ifExists);

void collectAtoms(xcb_atom_t *atom);

#endif
