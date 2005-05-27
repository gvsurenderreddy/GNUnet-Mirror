/*
     This file is part of GNUnet.
     (C) 2005 Christian Grothoff (and other contributing authors)

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
 * @file conf/wizard_curs.c
 * @brief A easy-to-use configuration assistant for curses
 * @author Nils Durner
 */

#include "gnunet_util.h"

#define LKC_DIRECT_LINK
#include "lkc.h"

#include "mconf_dialog.h"
#include "wizard_util.h"

extern int cols, rows;
int mconf_main(int ac, char **av);

static struct dialog_list_item **nic_items;
static int nic_item_count = 0;

void insert_nic_curs(char *name, int defaultNIC)
{
	struct dialog_list_item *item;

	/* Copy NIC data */	
	nic_items = (nic_item_count) ?
		realloc(nic_items, (nic_item_count + 1) * sizeof(struct dialog_list_item *)) :
		malloc(sizeof(struct dialog_list_item *));

	item = malloc(sizeof(struct dialog_list_item));
	memset(item, 0, sizeof(struct dialog_list_item));
	nic_items[nic_item_count] = item;
	item->name = strdup(name);
	item->namelen = strlen(name);
  item->selected = wiz_is_nic_default(name, defaultNIC);

	nic_item_count++;
}

int wizard_curs_main(int argc, char *argv[])
{
  const char * LANG;
  char * configFile;
  void *active_ptr = NULL;
	int idx, ret, autostart = 0, adv = 0;
	struct symbol *sym;
	char *defval, *user_name = NULL, *group_name = NULL;

  LANG = getenv("LANG");
  if (LANG == NULL)
      LANG = "en";
  if (strncmp(LANG, "en", 2) == 0)
      LANG = NULL;
  configFile = MALLOC(strlen(DATADIR"/config.in") + 4);
  strcpy(configFile,
                DATADIR"/config.in");   
  if (LANG != NULL) {
      strcat(configFile, ".");
      strncat(configFile,
                      LANG,
                      2);
  }

  conf_parse(configFile);
  
  conf_read(NULL);

  init_dialog();
  init_wsize();
	dialog_clear();

	if (dialog_msgbox(_("GNUnet configuration"), 
	  	_("Welcome to GNUnet!\n\nThis assistant will ask you a few basic questions "
	  	  "in order to configure GNUnet.\n\nPlease visit our homepage at\n\t"
	  	  "http://www.gnunet.org\nand join our community at\n\t"
	  	  "http://www.gnunet.org/drupal/\n\nHave a lot of fun,\n\nthe GNUnet team"),
	  	rows, cols - 5, 1) == -1)
		goto end;

	dialog_clear();
  	
  wiz_enum_nics(insert_nic_curs);

	/* Network interface */
	while (true) {
		ret = dialog_menu(_("GNUnet configuration"),
						_("Choose the network interface that connects your computer to "
							"the internet from the list below."), rows, cols - 5, 10,
						0, active_ptr, nic_item_count - 1, nic_items);
		
		if (ret == 2) {
			/* Help */
			dialog_msgbox(_("Help"), _("The \"Network interface\" is the device "
				"that connects your computer to the internet. This is usually a modem, "
				"an ISDN card or a network card in case you are using DSL."), rows,
				cols - 5, 1);
		}
		else if (ret <= 1) {
			/* Select or Exit */
			char *dst;
#ifdef MINGW
			char nic[21];
#else
			char *nic;
#endif
			for(idx = 0; idx < nic_item_count; idx++) {
				
				if (nic_items[idx]->selected) {
#ifdef MINGW
					char *src = strrchr(nic_items[idx]->name, '-') + 2;
					dst = nic;
					while(*src)
						*dst++ = *src++;
					dst[-1] = 0;
#else
					nic = nic_items[idx]->name;
#endif
					sym = sym_lookup("INTERFACE", "NETWORK", 0);
					sym_set_string_value(sym, nic);
					sym = sym_lookup("INTERFACES", "LOAD", 0);
					sym_set_string_value(sym, nic);
				}
				
				free(nic_items[idx]->name);
				free(nic_items[idx]);
			}
			free(nic_items);
			
			break;
		}
	}
	
	if (ret == 1 || ret == -1)
		goto end;
	
	dialog_clear();
	
	/* IP address */
	if ((sym = sym_find("IP", "NETWORK"))) {
		sym_calc_value_ext(sym, 1);
		defval = (char *) sym_get_string_value(sym);
	}
	else
		defval = NULL;
	
	while(true) {
		ret = dialog_inputbox(_("GNUnet configuration"), _("What is this computer's "
			"public IP adress or hostname?\n\nIf in doubt, leave this empty."),
			rows, cols - 5, defval ? defval : "");
		
		if (ret == 1) {
			/* Help */
			dialog_msgbox(_("Help"), _("If your provider always assigns the same "
				"IP-Address to you (a \"static\" IP-Address), enter it into the "
				"\"IP-Address\" field. If your IP-Address changes every now and then "
				"(\"dynamic\" IP-Address) but there's a hostname that always points "
				"to your actual IP-Address (\"Dynamic DNS\"), you can also enter it "
				"here.\nIf in doubt, leave the field empty. GNUnet will then try to "
				"determine your IP-Address."), rows, cols - 5, 1);
		}
		else if (ret <= 0)
			break;
	}
	
	if (ret == -1)
		goto end;
		
	sym_set_string_value(sym, dialog_input_result);
	
	dialog_clear();

	/* NAT? */
	while(true) {
		ret = dialog_yesno(_("GNUnet configuration"), _("Is this machine behind "
				"NAT?\n\nIf you are connected to the internet through another computer "
				"doing SNAT, a router or a \"hardware firewall\" and other computers "
				"on the internet cannot connect to this computer, say \"yes\" here. "
				"Answer \"no\" on direct connections through modems, ISDN cards and "
				"DNAT (also known as \"port forwarding\")."), rows, cols - 5);
		
		if (ret != -2)
			break;
	}
	
	if (ret == -1)
		goto end;
	else
		sym_set_tristate_value(sym, !ret); /* ret is inverted */
	
	/* Upstream */
	if ((sym = sym_find("MAXNETUPBPSTOTAL", "LOAD"))) {
		sym_calc_value_ext(sym, 1);
		defval = (char *) sym_get_string_value(sym);
	}
	else
		defval = NULL;

	while(true) {
		ret = dialog_inputbox(_("GNUnet configuration"), _("How much upstream "
			"(Bytes/s) may be used?"), rows, cols - 5, defval ? defval : "");
		
		if (ret == 1) {
			/* Help */
			dialog_msgbox(_("Help"), _("You can limit GNUnet's ressource usage "
				"here.\n\nThe \"upstream\" is the data channel through which data "
				"is *sent* to the internet. The limit is either the total maximum "
				"for this computer or how much GNUnet itself is allowed to use. You "
				"can specify that later. If you have a flatrate, you can set it to "
				"the maximum speed of your internet connection."), rows, cols - 5, 1);
		}
		else if (ret <= 0)
			break;
	}

	if (ret == -1)
		goto end;

	sym_set_string_value(sym, dialog_input_result);

	dialog_clear();

	/* Downstram */
	if ((sym = sym_find("MAXNETDOWNBPSTOTAL", "LOAD"))) {
		sym_calc_value_ext(sym, 1);
		defval = (char *) sym_get_string_value(sym);
	}
	else
		defval = NULL;

	while(true) {
		ret = dialog_inputbox(_("GNUnet configuration"), _("How much downstream "
			"(Bytes/s) may be used?"), rows, cols - 5, defval ? defval : "");
		
		if (ret == 1) {
			/* Help */
			dialog_msgbox(_("Help"), _("You can limit GNUnet's ressource usage "
				"here.\n\nThe \"downstream\" is the data channel through which data "
				"is *received* from the internet. The limit is either the total maximum "
				"for this computer or how much GNUnet itself is allowed to use. You "
				"can specify that later. If you have a flatrate you can set it to "
				"the maximum speed of your internet connection."), rows, cols - 5, 1);
		}
		else if (ret <= 0)
			break;
	}
	
	if (ret == -1)
		goto end;

	sym_set_string_value(sym, dialog_input_result);

	dialog_clear();

	/* Bandwidth allocation */
	while (true) {
		ret = dialog_yesno(_("GNUnet configuration"), _("Share denoted bandwidth "
				"with other applications?\n\nSay \"yes\" here, if you don't want other "
				"network traffic to interfere with GNUnet's operation, but still wish to "
				"constrain GNUnet's bandwidth usage to values entered in the previous "
				"steps, or if you can't reliably measure the maximum capabilities "
				"of your connection. \"No\" can be very useful if other applications "
				"are causing a lot of traffic on your LAN.  In this case, you do not "
				"want to limit the traffic that GNUnet can inflict on your internet "
				"connection whenever your high-speed LAN gets used (e.g. by NFS)."),
				rows, cols - 5);
		
		if (ret != -2)
			break;
	}
	
	if (ret == -1)
		goto end;
	else
		sym_set_tristate_value(sym, !ret); /* ret is inverted */
	
	dialog_clear();

	/* Max CPU */
	if ((sym = sym_find("MAXCPULOAD", "LOAD"))) {
		sym_calc_value_ext(sym, 1);
		defval = (char *) sym_get_string_value(sym);
	}
	else
		defval = NULL;

	while(true) {
		ret = dialog_inputbox(_("GNUnet configuration"), _("How much CPU (in %) may "
			"be used?"), rows, cols - 5, defval ? defval : "");
		
		if (ret == 1) {
			/* Help */
			dialog_msgbox(_("Help"), _("You can limit GNUnet's ressource usage "
				"here.\n\nThis is the percentage of processor time GNUnet is allowed "
				"to use."), rows, cols - 5, 1);
		}
		else if (ret <= 0)
			break;
	}
	
	if (ret == -1)
		goto end;

	sym_set_string_value(sym, dialog_input_result);
	
	dialog_clear();

	/* Migration */
	while(true) {
		ret = dialog_yesno(_("GNUnet configuration"), _("Store migrated content?"
				"\n\nGNUnet is able to store data from other peers in your datastore. "
				"This is useful if an adversary has access to your inserted content and "
				"you need to deny that the content is yours. With \"content migration\" "
				"on, the content could have \"migrated\" over the internet to your node"
				" without your knowledge.\nIt also helps to spread popular content over "
				"different peers to enhance availability."), rows, cols - 5);
				
		if (ret != -2)
			break;
	}

	if (ret == -1)
		goto end;
	else
		sym_set_tristate_value(sym, !ret); /* ret is inverted */

	dialog_clear();

	/* Quota */
	if ((sym = sym_find("DISKQUOTA", "FS"))) {
		sym_calc_value_ext(sym, 1);
		defval = (char *) sym_get_string_value(sym);
	}
	else
		defval = NULL;

	while(true) {
		ret = dialog_inputbox(_("GNUnet configuration"), _("What's the maximum "
			"datastore size in MB?\n\nThe GNUnet datastore contains all data that "
			"GNUnet generates (index data, inserted and migrated content)."),
			rows, cols - 5, defval ? defval : "");
		
		if (ret == 1) {
			/* Help - not available */
		}
		else if (ret <= 0)
			break;
	}
	
	if (ret == -1)
		goto end;

	sym_set_string_value(sym, dialog_input_result);

	dialog_clear();

	/* Autostart */
	if (wiz_autostart_capable()) {
		while(true) {
			ret = dialog_yesno(_("GNUnet configuration"), _("Do you want to launch "
					"GNUnet as a system service?"
					"\n\nIf you say \"yes\" here, the GNUnet background process will be "
					"automatically started when you turn on your computer. If you say \"no\""
					" here, you have to launch GNUnet yourself each time you want to use it."),
					rows, cols - 5);
					
			if (ret != -2)
				break;
		}
	
		if (ret == -1)
			goto end;
		else
			autostart = !ret; /* ret is inverted */
	
		dialog_clear();
	}
	
	/* User */
	if (wiz_useradd_capable()) {
		while(true) {
			ret = dialog_inputbox(_("GNUnet configuration"),
				_("Define the user owning the GNUnet service.\n\n"
					"For security reasons, it is a good idea to let this setup create "
					"a new user account under which the GNUnet service is started "
					"at system startup.\n\n"
					"You can also specify an already existant user account here.\n\n"
					"In any case, you should check its permissions to critical files "
					"on your system.\n\nGNUnet user:"),
				rows, cols - 5, "gnunet");
			
			if (ret == 1) {
				/* Help */
			}
			else if (ret <= 0) {
				user_name = strdup("gnunet");
				break;
			}
		}
		
		if (ret == -1)
			goto end;

		dialog_clear();

		/* Group */
		if (wiz_groupadd_capable()) {
			while(true) {
				ret = dialog_inputbox(_("GNUnet configuration"),
					_("Define the group owning the GNUnet service.\n\n"
						"For security reasons, it is a good idea to let this setup create "
						"a new group for the chosen user account.\n\n"
						"You can also specify a already existant group here.\n\n"
						"Only members of this group will be allowed to start and stop the "
						"the GNUnet server and have access to GNUnet server data.\n\n"
						"GNUnet group:"),
					rows, cols - 5, "gnunet");
				
				if (ret == 1) {
					/* Help */
				}
				else if (ret <= 0) {
					group_name = strdup(dialog_input_result);
					break;
				}
			}
		
			if (ret == -1)
				goto end;
	
			dialog_clear();
		}
	}

	dialog_clear();

	/* Advanced */
	while(true) {
		ret = dialog_yesno(_("GNUnet configuration"), _("If you are an experienced "
				"user, you may want to tweak your GNUnet installation using the enhanced "
				"configurator.\n\nDo you want to start it after saving your configuration?"),
				rows, cols - 5);
				
		if (ret != -2)
			break;
	}
	
	if (ret == -1)
		goto end;
	else
		adv = !ret;

	dialog_clear();
  end_dialog();

	/* Save config */
	if (user_name && strlen(user_name) > 0)
		wiz_addServiceAccount(group_name, user_name);

	wiz_autostart(autostart, user_name, group_name);

	init_dialog();
	dialog_clear();

	while(true) {
		if (conf_write(NULL) != 0) {
			ret = dialog_yesno(_("GNUnet configuration"),
							_("Cannot save configuration.\n\nTry again?"), rows, cols - 5);
		}
		else
			ret = 1;
			
		if (ret == 1 || ret == -1)
			break;
	}
	
end:
  end_dialog();
  
  if (user_name)
  	free(user_name);
  if (group_name)
  	free(group_name);

	if (adv) {
		mconf_main(argc, argv);
	}

  return 0;
}

/* end of wizard_curs.c */
