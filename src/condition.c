
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "comm.h"
#include "db.h"
#include "events.h"
#include "interp.h"
#include "justice.h"
#include "new_combat_def.h"
#include "objmisc.h"
#include "prototypes.h"
#include "spells.h"
#include "structs.h"
#include "utils.h"
#include "weather.h"
#include "damage.h"

extern P_char character_list;
extern P_desc descriptor_list;
extern P_index mob_index;
extern P_index obj_index;
extern P_obj object_list;
extern P_room world;
extern int proccing_slots[];
extern struct material_data materials[];
char bufCOLOR[MAX_STRING_LENGTH];

char  *item_condition(P_obj obj)
{
  int      value;
  float compare;
  static char buf[MAX_STRING_LENGTH];

#ifndef NEW_COMBAT
  value = BOUNDED(1, obj->condition / obj->max_condition, 12);
#else
  value =
    BOUNDED(1, (int) (((float) obj->curr_sp / (float) obj->max_sp) * 10.0),
            12);
#endif

  if (obj->type == ITEM_CORPSE)
    value = 12;                 /* So these item types wont be shown with a condition */

  if(obj->condition / obj->max_condition > .900)
    sprintf(bufCOLOR, "&+G");
  else if(obj->condition / obj->max_condition > .700)
    sprintf(bufCOLOR, "&+g");
  else if(obj->condition / obj->max_condition > .500)
    sprintf(bufCOLOR, "&+y");
  else if(obj->condition / obj->max_condition > .200)
    sprintf(bufCOLOR, "&+r");
  else if(obj->condition / obj->max_condition > 0)
    sprintf(bufCOLOR, "&+R");

  compare = ((float) obj->condition / obj->max_condition);

  if(compare < 0.900)
  sprintf(buf, " [%s%d&n%]             ", bufCOLOR, (int) (100 * ((float) obj->condition / obj->max_condition)));
  else
  sprintf(buf, "");

  return buf;
}

const char *item_damage_messages[][2] = {
  {"was damaged from the massive blow!",
   "was completely destroyed by the massive blow!"},
  {"is burned from the blast!",
   "melted from the intense heat!"},
  {"is weakened from the intense cold!",
   "freezes and shatters into million pieces from the intense cold!"},
  {"is charred by electrical energy!",
   "exploded spreading charges of electrical energy!"},
  {"was corroded by gas.",
   "crumbled as it was consumed by the toxic gas."},
  {"was corroded by acid.",
   "melted, corroded by the acid."},
  {"is blasted by the negative energy!",
   "was disintegrated by the negative energy!"},
  {"is burned by the light!",
   "shattered into a million pieces by the divine fury!"},
  {"cracks from the psionic assault!",
   "shattered in bits as the psionic assault became too much!"},
  {"is corrupted by spiritual anger!",
   "was destroyed by the blast!"},
  {"cracks attacked by the harmonic resonance.",
   "shattered into pieces as the vibrations became too much!"},   
};

int DamageOneItem(P_char ch, int dam_type, P_obj obj, bool destroy)
{
  int      num;
  char     buf[MAX_STRING_LENGTH];
  int      objtype = GET_ITEM_TYPE(obj);
  bool     force_destroy = destroy;

  if((objtype == ITEM_TOTEM) ||
     (objtype == ITEM_KEY) ||
     (objtype == ITEM_SPELLBOOK) ||
     (IS_ARTIFACT(obj)))
  {
    return 0;
  }
  
  num = number(3, 9);
  
  if(materials[obj->material].dam_res[dam_type])
  {
    num *= materials[obj->material].dam_res[dam_type];
  }
  
  // physical being the most common is less harsh
  if(dam_type == SPLDAM_GENERIC)
  {
    num >>= 1;
  }
  
  // Lets reduce the amount of damage a lance takes.
  if(objtype == ITEM_WEAPON &&
    obj->value[0] == 16)
  {
    num = number(1, 3);
  }
  
  obj->condition -= num;
  
  if(obj->condition < 0)
  {
    destroy = TRUE;
  }

  sprintf(buf, "Your $q %s",
          item_damage_messages[dam_type - 1][destroy ? 1 : 0]);
  act(buf, TRUE, ch, obj, 0, TO_CHAR);

  sprintf(buf, "$n's $q %s",
          item_damage_messages[dam_type - 1][destroy ? 1 : 0]);
  act(buf, TRUE, ch, obj, 0, TO_ROOM);

  if(destroy)
    if(!force_destroy)
      MakeScrap(ch, obj);
    else
      extract_obj(obj, TRUE);

  return destroy;
}


void MakeScrap(P_char ch, P_obj obj)
{
  char     buf[MAX_STRING_LENGTH];
  P_obj    t, x;
  int      pos;


  if (!ch || !obj || (ch->in_room == NOWHERE))
    return;

  act("$p falls to the ground in scraps.", TRUE, ch, obj, 0, TO_CHAR);
  act("$p falls to the ground in scraps.", TRUE, ch, obj, 0, TO_ROOM);

  t = read_object(9, VIRTUAL);

  if (!t)
    return;

  sprintf(buf, "Scraps from %s&n lie in a pile here.",
          obj->short_description);

  t->description = str_dup(buf);

  sprintf(buf, "a pile of scraps from %s", obj->short_description);

  t->short_description = str_dup(buf);

  sprintf(buf, "%s scraps pile", obj->name);

  t->name = str_dup(buf);

  t->str_mask = STRUNG_DESC1 | STRUNG_DESC2 | STRUNG_KEYS;

  if (OBJ_CARRIED(obj))
  {
    obj_from_char(obj, TRUE);
  }
  else if (OBJ_WORN(obj))
  {
    for (pos = 0; pos < MAX_WEAR; pos++)
      if (ch->equipment[pos] == obj)
        break;
    if (pos >= MAX_WEAR)
    {
      logit(LOG_DEBUG, "MakeScrap(), can't find worn object in equip");
      raise(SIGSEGV);
    }
    obj = unequip_char(ch, pos);
  }

  set_obj_affected(t, 400, TAG_OBJ_DECAY, 0);
  obj_to_room(t, ch->in_room);

  while (obj->contains)
  {
    x = obj->contains;
    obj_from_obj(x);
    obj_to_room(x, ch->in_room);
  }

  if (OBJ_FALLING(t))
    falling_obj(t, 1, false);

  extract_obj(obj, TRUE);
}

void DamageAllStuff(P_char ch, int dam_type)
{
  int      j;
  P_obj    obj, next;

  /* this procedure takes all of the items in equipment and inventory
     and damages the ones that should be damaged */

  /* equipment */

  for (j = 0; j < MAX_WEAR; j++)
  {
    if (ch->equipment[j] && (j != WIELD) && (j != WIELD2))
    {
      obj = ch->equipment[j];
      DamageOneItem(ch, dam_type, obj, FALSE);
    }
  }

  /* inventory */
  for (obj = ch->carrying; obj; obj = next)
  {
    next = obj->next_content;
    DamageOneItem(ch, dam_type, obj, FALSE);
  }
}


void DamageStuff(P_char v, int type)
{
  int      slot;
  P_obj    obj;

  slot = number(2, CUR_MAX_WEAR);
  if (v->equipment[slot] && slot != WIELD && slot != WIELD2)
  {
    DamageOneItem(v, type, v->equipment[slot], FALSE);
  }
}
