/*
     This file is part of GNUnet

     GNUnet is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     GNUnet is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with GNUnet; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
*/

/**
 * @file applications/afs/gtkui/main.h
 * @author Igor Wronsky
 **/

#ifndef GTKUI_MAIN_H
#define GTKUI_MAIN_H

/**
 * This semaphore can be up'ed by threads that will
 * need the GUI and that want to prevent gnunet-gtk
 * from exiting while they are running.
 **/
extern Semaphore * refuseToDie;

extern GtkItemFactory * itemFactory;

void refreshMenuSensitivity();

#endif
