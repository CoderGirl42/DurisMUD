/*
 * specs.arih.c - professor arih, the shitty npc
 * a chaotic trickster mob loaded only by gods for fun/events
 */

#include <stdio.h>
#include <string.h>

#include "comm.h"
#include "db.h"
#include "events.h"
#include "interp.h"
#include "prototypes.h"
#include "spells.h"
#include "specs.prototypes.h"
#include "structs.h"
#include "utils.h"
#include "disguise.h"
#include "specs.arih.h"

extern P_room world;
extern P_index obj_index;

#define VOBJ_POLYJUICE_POTION 1261

// give arih his polyjuice potions if he doesnt have any
static void arih_init_potions(P_char ch)
{
  P_obj obj, potion;
  int count = 0;

  // count how many polyjuice potions we have
  for (obj = ch->carrying; obj; obj = obj->next_content)
  {
    if (OBJ_VNUM(obj) == VOBJ_POLYJUICE_POTION)
      count++;
  }

  // give potions if we have less than 2
  while (count < 2)
  {
    potion = read_object(VOBJ_POLYJUICE_POTION, VIRTUAL);
    if (potion)
      obj_to_char(potion, ch);
    count++;
  }
}

/*
 * professor arih - chaotic trickster mob
 *
 * idle: says rude stuff, drinks polyjuice to look like random players
 * fighting: yells themed quotes and casts nasty spells
 *
 * dont autoload this. gods load it manually for shenanigans
 */
int professor_arih(P_char ch, P_char pl, int cmd, char *arg)
{
  P_char vict, target = NULL;
  int pc_count = 0;
  int random_pc = 0;

  if (cmd == CMD_SET_PERIODIC)
  {
    // init potions on periodic tick
    arih_init_potions(ch);
    return TRUE;
  }

  if (!ch || !IS_AWAKE(ch))
    return FALSE;

  // let normal stuff happen
  if (cmd)
    return FALSE;

  // fighting time
  if (IS_FIGHTING(ch))
  {
    // pick someone to mess with
    target = pick_target(ch, PT_TOLERANT);
    if (!target)
      target = GET_OPPONENT(ch);
    if (!target)
      return FALSE;

    switch (number(1, 20))
    {
    case 1:
    case 2:
      // curse + blind everyone lol
      mobsay(ch, "What are you looking at? Heh, I curse you to go blind!");
      act("$n cackles with malicious glee!", FALSE, ch, 0, 0, TO_ROOM);
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
      {
        if (IS_PC(vict) && !IS_TRUSTED(vict) && vict != ch)
        {
          spell_curse(60, ch, 0, SPELL_TYPE_SPELL, vict, 0);
          spell_blindness(60, ch, 0, SPELL_TYPE_SPELL, vict, 0);
        }
      }
      return TRUE;

    case 3:
      // avada kedavra!
      mobsay(ch, "Take this! AVADA KEDAVRA!");
      act("&+GA sickly green light erupts from&n $n&+G's hands!&n", FALSE, ch, 0, 0, TO_ROOM);
      spell_pword_kill(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 4:
      // call of the wild
      mobsay(ch, "Arih says... TRANSFORM!");
      act("$n waves $s hands in an exaggerated fashion!", FALSE, ch, 0, 0, TO_ROOM);
      spell_call_of_the_wild(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 5:
      // feeblemind
      mobsay(ch, "Who do you think you are, Grandma Lykria? Let me fix that brain of yours!");
      spell_feeblemind(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 6:
      // energy drain
      mobsay(ch, "Expelliarmus! Wait, wrong spell... DRAIN YOUR SOUL!");
      act("&+L$n's eyes glow with an unholy light!&n", FALSE, ch, 0, 0, TO_ROOM);
      spell_energy_drain(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 7:
    case 8:
      // fireball
      mobsay(ch, "INCENDIO! No wait... BIGGER!");
      act("&+r$n grins maniacally as &+Rflames&+r gather in $s palms!&n", FALSE, ch, 0, 0, TO_ROOM);
      spell_fireball(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 9:
      // curse everyone
      mobsay(ch, "You're ALL terrible! Have some bad luck!");
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
      {
        if (IS_PC(vict) && !IS_TRUSTED(vict) && vict != ch)
          spell_curse(60, ch, 0, SPELL_TYPE_SPELL, vict, 0);
      }
      return TRUE;

    case 10:
      // stun
      mobsay(ch, "PETRIFICUS TOTALUS! Or whatever...");
      MobCastSpell(ch, target, 0, SPELL_PWORD_STUN, 60);
      return TRUE;

    case 11:
      // blind
      mobsay(ch, "Constant vigilance? NAH! BLINDNESS FOR YOU!");
      MobCastSpell(ch, target, 0, SPELL_PWORD_BLIND, 60);
      return TRUE;

    case 12:
      // harm
      mobsay(ch, "This might sting a little... OKAY IT'LL HURT A LOT!");
      spell_harm(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 13:
      // dispel
      mobsay(ch, "Nice buffs you got there... would be a shame if someone... DISPELLED THEM!");
      spell_dispel_magic(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 14:
      // lightning bolt
      mobsay(ch, "UNLIMITED POWER!!!");
      act("&+BElectricity crackles around&n $n!", FALSE, ch, 0, 0, TO_ROOM);
      spell_lightning_bolt(60, ch, 0, SPELL_TYPE_SPELL, target, 0);
      return TRUE;

    case 15:
      // earthquake
      mobsay(ch, "The ground beneath you is MINE to command!");
      spell_earthquake(60, ch, 0, SPELL_TYPE_SPELL, 0, 0);
      return TRUE;

    case 16:
      // random insults
      switch(number(1, 5))
      {
        case 1: mobsay(ch, "Is that all you've got? My grandmother hits harder!"); break;
        case 2: mobsay(ch, "I've seen slimes with better combat skills!"); break;
        case 3: mobsay(ch, "Did your mother drop you on your head? Multiple times?"); break;
        case 4: mobsay(ch, "You call that an attack? Pathetic!"); break;
        case 5: mobsay(ch, "I'm not even trying and I'm winning!"); break;
      }
      return TRUE;

    default:
      // do nothing, normal combat
      return FALSE;
    }
  }

  // idle stuff - also check potions here as fallback
  arih_init_potions(ch);

  switch (number(1, 50))
  {
  case 1:
    mobsay(ch, "What are YOU looking at?");
    do_action(ch, 0, CMD_GLARE);
    return TRUE;

  case 2:
    mobsay(ch, "I didn't ask for your opinion.");
    do_action(ch, 0, CMD_SNORT);
    return TRUE;

  case 3:
    mobsay(ch, "Ugh, mortals. So... mortal.");
    do_action(ch, 0, CMD_SIGH);
    return TRUE;

  case 4:
    mobsay(ch, "Don't you have somewhere else to be? Preferably far from me?");
    return TRUE;

  case 5:
    mobsay(ch, "I've turned better wizards than you into ferrets.");
    do_action(ch, 0, CMD_CACKLE);
    return TRUE;

  case 6:
    mobsay(ch, "CONSTANT VIGILANCE!");
    act("$n looks around suspiciously.", FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;

  case 7:
    mobsay(ch, "You remind me of someone I used to hate. Still do, actually.");
    return TRUE;

  case 8:
    mobsay(ch, "Is it just me, or does this place smell like incompetence?");
    do_action(ch, 0, CMD_SNIFF);
    return TRUE;

  case 9:
  case 10:
    // polyjuice time - disguise as random player
    pc_count = 0;
    for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
    {
      if (IS_PC(vict) && !IS_TRUSTED(vict) && vict != ch)
        pc_count++;
    }

    if (pc_count > 0)
    {
      random_pc = number(1, pc_count);
      pc_count = 0;
      for (vict = world[ch->in_room].people; vict; vict = vict->next_in_room)
      {
        if (IS_PC(vict) && !IS_TRUSTED(vict) && vict != ch)
        {
          pc_count++;
          if (pc_count == random_pc)
          {
            target = vict;
            break;
          }
        }
      }

      if (target)
      {
        mobsay(ch, "Hmm, I wonder what it's like to be... YOU!");
        act("&+m$n pulls out a flask of &+Mbubbling potion&+m and drinks it!&n", FALSE, ch, 0, 0, TO_ROOM);
        act("&+M$n's features begin to shift and change...&n", FALSE, ch, 0, 0, TO_ROOM);

        // set up disguise - same way disguise.c does it for PC targets
        IS_DISGUISE_PC(ch) = TRUE;
        IS_DISGUISE_NPC(ch) = FALSE;
        IS_DISGUISE_ILLUSION(ch) = FALSE;
        IS_DISGUISE_SHAPE(ch) = FALSE;
        str_free(ch->disguise.name);
        ch->disguise.name = str_dup(GET_NAME(target));
        ch->disguise.m_class = target->player.m_class;
        ch->disguise.race = GET_RACE(target);
        ch->disguise.level = GET_LEVEL(target);
        ch->disguise.hit = 9999;  // very durable
        ch->disguise.racewar = GET_RACEWAR(target);
        str_free(ch->disguise.title);
        if (GET_TITLE(target))
          ch->disguise.title = str_dup(GET_TITLE(target));

        act("&+M$n now looks exactly like $N!&n", FALSE, ch, 0, target, TO_NOTVICT);
        act("&+M$n now looks exactly like YOU!&n", FALSE, ch, 0, target, TO_VICT);
        mobsay(ch, "Ooh, not bad! I make a handsome devil, don't I?");
        do_action(ch, 0, CMD_CACKLE);
        return TRUE;
      }
    }
    // nobody to copy
    mobsay(ch, "Hmm, nobody interesting to impersonate here...");
    do_action(ch, 0, CMD_POUT);
    return TRUE;

  case 11:
    // remove disguise
    if (IS_DISGUISE(ch))
    {
      mobsay(ch, "Ugh, being someone else is exhausting. Back to my beautiful self!");
      act("&+C$n's features shimmer and return to normal.&n", FALSE, ch, 0, 0, TO_ROOM);
      remove_disguise(ch, FALSE);
      return TRUE;
    }
    return FALSE;

  case 12:
    mobsay(ch, "Did you hear that? No? Good, it was probably nothing... or WAS it?");
    act("$n looks around nervously.", FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;

  case 13:
    mobsay(ch, "I could turn you all into newts. Don't tempt me.");
    return TRUE;

  case 14:
    mobsay(ch, "In my day, adventurers had RESPECT. And better hygiene.");
    return TRUE;

  case 15:
    act("$n mutters something about 'kids these days' under $s breath.", FALSE, ch, 0, 0, TO_ROOM);
    return TRUE;

  default:
    return FALSE;
  }

  return FALSE;
}
