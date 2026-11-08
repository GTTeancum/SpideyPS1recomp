/*
 * The Xbox title identity: the one place it is defined. xbox/tools/patch_xbe.py reads
 * the ID from here to write it into the built XBE's certificate (nxdk's cxbe always
 * writes 0xFFFF0002, shared by every nxdk program), and runtime/card.c keeps saves in
 * E:\UDATA\<ID>, the folder the dashboard lists for this title.
 *
 * Retail title IDs begin with two upper-case letters (the publisher); "os" in lower
 * case (OpenSpidey) cannot collide with a retail game.
 */
#pragma once

#define OPENSPIDEY_TITLE_ID 0x6F730001u
#define OPENSPIDEY_TITLE_ID_HEX "6F730001"
#define OPENSPIDEY_TITLE_NAME "OpenSpidey"
