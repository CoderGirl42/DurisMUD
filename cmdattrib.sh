#!/bin/bash

#Parses the file for attribs
parsefile ( )
{
  declare -i COUNT number
  declare -i COUNT1 number
  declare -i COUNT2 number
  declare -i FOUNDIT number
  FOUNDIT=0

  FILENAME=`grep -l "$FUNCTIONNAME" ./src/*.c`
  echo $FUNCTIONNAME

  cat $FILENAME | 
  while read LINE;
  do
    if [[ $LINE = "$FUNCTIONNAME"* ]]; then
      # Count the damn brackets and do the math
      COUNT1=`grep -o "{" <<<"$LINE" | wc -l`
      COUNT2=`grep -o "}" <<<"$LINE" | wc -l`
      COUNT=$COUNT1-$COUNT2
      # FOUNDIT -> We're inside the function.
      FOUNDIT=1
    elif [[ $FOUNDIT = 1 ]]; then
      # Hunt for the GET_C_...( crap.
      ATTRIB=$ATTRIB grep -o "GET_C_...(.." <<<"$LINE"
      # Count the damn brackets and do the math
      COUNT1=`grep -o "{" <<<"$LINE" | wc -l`
      COUNT2=`grep -o "}" <<<"$LINE" | wc -l`
      COUNT+=$COUNT1-$COUNT2

      # if at end of function
      if [[ $COUNT = 0 ]]; then
        break
      fi
    fi
  done

  echo $ATTRIB
}

#remove the old command_att. file.
rm -f command_attributes.txt

FUNCTIONNAME="void do_apply_poison"
parsefile

FUNCTIONNAME="void do_appraise"
parsefile

FUNCTIONNAME="void armlock_check"
parsefile

FUNCTIONNAME="void do_awareness"
parsefile

FUNCTIONNAME="bool backstab"
parsefile

FUNCTIONNAME="int bard_calc_chance"
parsefile

FUNCTIONNAME="void event_barrage"
parsefile

FUNCTIONNAME="void bash"
parsefile

FUNCTIONNAME="void battle_orders"
parsefile

FUNCTIONNAME="void event_bearhug"
parsefile

FUNCTIONNAME="void do_berserk"
parsefile

FUNCTIONNAME="void do_blood_scent"
parsefile

FUNCTIONNAME="void do_bodyslam"
parsefile

FUNCTIONNAME="void branch"
parsefile

FUNCTIONNAME="void buck"
parsefile

FUNCTIONNAME="void do_capture"
parsefile

FUNCTIONNAME="void do_carve"
parsefile

FUNCTIONNAME="void do_chi"
parsefile

FUNCTIONNAME="void event_combination"
parsefile

FUNCTIONNAME="void do_chant"
parsefile

FUNCTIONNAME="void do_charge"
parsefile

FUNCTIONNAME="bool circle"
parsefile

FUNCTIONNAME="void do_conjure"
parsefile

FUNCTIONNAME="void do_craft"
parsefile

FUNCTIONNAME="void do_dirttoss"
parsefile

FUNCTIONNAME="void do_disarm"
parsefile

FUNCTIONNAME="void do_disguise"
parsefile

FUNCTIONNAME="void do_drag"
parsefile

FUNCTIONNAME="void do_dragon_punch"
parsefile

FUNCTIONNAME="void do_enhance"
parsefile

FUNCTIONNAME="void do_feign_death"
parsefile

FUNCTIONNAME="bool flank"
parsefile

FUNCTIONNAME="void do_flee"
parsefile

FUNCTIONNAME="void do_flurry_of_blows"
parsefile

FUNCTIONNAME="void do_forage"
parsefile

FUNCTIONNAME="void do_garrote"
parsefile

FUNCTIONNAME="void gaze"
parsefile

FUNCTIONNAME="void do_groundslam"
parsefile

FUNCTIONNAME="void do_hamstring"
parsefile

FUNCTIONNAME="void do_headbutt"
parsefile

FUNCTIONNAME="void event_headlock"
parsefile

FUNCTIONNAME="P_char guard_check"
parsefile

FUNCTIONNAME="void do_hide"
parsefile

FUNCTIONNAME="void do_hit"
parsefile

FUNCTIONNAME="void do_hitall"
parsefile

FUNCTIONNAME="void do_holy_smite"
parsefile

FUNCTIONNAME="void do_infuriate"
parsefile

FUNCTIONNAME="void chant_jin_touch"
parsefile

FUNCTIONNAME="void kick"
parsefile

FUNCTIONNAME="void do_layhand"
parsefile

FUNCTIONNAME="void event_leglock"
parsefile

FUNCTIONNAME="void do_listen"
parsefile

FUNCTIONNAME="void maul"
parsefile

FUNCTIONNAME="void do_mug"
parsefile

FUNCTIONNAME="void do_murder"
parsefile

FUNCTIONNAME="void do_ogre_roar"
parsefile

FUNCTIONNAME="void parlay"
parsefile

FUNCTIONNAME="void do_quaff"
parsefile

FUNCTIONNAME="void chant_quivering_palm"
parsefile

FUNCTIONNAME="void do_rage"
parsefile

FUNCTIONNAME="void do_rampage"
parsefile

FUNCTIONNAME="void do_recite"
parsefile

FUNCTIONNAME="void rescue"
parsefile

FUNCTIONNAME="void restrain"
parsefile

FUNCTIONNAME="void do_rearkick"
parsefile

FUNCTIONNAME="void do_retreat"
parsefile

FUNCTIONNAME="void do_riff"
parsefile

FUNCTIONNAME="int chance_roundkick"
parsefile

FUNCTIONNAME="void rush"
parsefile

FUNCTIONNAME="void do_search"
parsefile

FUNCTIONNAME="void do_shadowstep"
parsefile

FUNCTIONNAME="void shieldpunch"
parsefile

FUNCTIONNAME="void do_shriek"
parsefile

FUNCTIONNAME="void do_smith"
parsefile

FUNCTIONNAME="void do_sneak"
parsefile

FUNCTIONNAME="void event_sneaky_strike"
parsefile

FUNCTIONNAME="void do_springleap"
parsefile

FUNCTIONNAME="void do_stampede"
parsefile

FUNCTIONNAME="void do_stance"
parsefile

FUNCTIONNAME="void do_steal"
parsefile

FUNCTIONNAME="void do_newsteal"
parsefile

FUNCTIONNAME="void do_subterfuge"
parsefile

FUNCTIONNAME="void do_sweeping_thrust"
parsefile

FUNCTIONNAME="void do_tackle"
parsefile

FUNCTIONNAME="int takedown_check"
parsefile

FUNCTIONNAME="void do_throat_crush"
parsefile

FUNCTIONNAME="void do_throw_potion"
parsefile

FUNCTIONNAME="void do_trample"
parsefile

FUNCTIONNAME="void do_trap"
parsefile

FUNCTIONNAME="void do_trip"
parsefile

FUNCTIONNAME="void do_use"
parsefile

FUNCTIONNAME="void do_war_cry"
parsefile

FUNCTIONNAME="void webwrap"
parsefile

FUNCTIONNAME="void do_whirlwind"
parsefile

exit

psionics.c:void do_drain(P_char ch, char *arg, int cmd)
psionics.c:void do_gith_neckbite(P_char ch, char *arg, int cmd)
psionics.c:void do_absorbe(P_char ch, char *arg, int cmd)
quest.c:void do_reload_quest(P_char ch, char *arg, int cmd)
randobj.c:void do_randobj(P_char ch, char *strn, int val)
range.c:void do_gather(P_char ch, char *argument, int cmd)
range.c:void do_fire(P_char ch, char *argument, int cmd)
range.c:void do_throw(P_char ch, char *argument, int cmd)
range.c:void do_load_weapon(P_char ch, char *argument, int cmd)
range.c:void do_cover(P_char ch, char *argument, int cmd)
rogues.c:void do_slip(P_char ch, char *argument, int cmd)
salchemist.c:void do_mix(P_char ch, char *argument, int cmd)
salchemist.c:void do_spellbind (P_char ch, char *argument, int cmd)
salchemist.c:void do_encrust(P_char ch, char *argument, int cmd)
salchemist.c:void do_fix(P_char ch, char *argument, int cmd)
salchemist.c:void do_smelt(P_char ch, char *arg, int cmd)
salchemist.c:void do_enchant(P_char ch, char *argument, int cmd)
shadow.c:void do_shadow(P_char ch, char *argument, int cmd)
shadow.c:void do_grapple(P_char ch, char *arg, int cmd) {
skills.c:extern void do_epic_reset(P_char ch, char *arg, int cmd);
skills.c:extern void do_epic_reset_norefund(P_char ch, char *arg, int cmd);
smagic.c:void do_point(P_char ch, P_char victim)
sparser.c:void do_will(P_char ch, char *argument, int cmd)
sparser.c:void do_cast(P_char ch, char *argument, int cmd)
specializations.c:void do_spec_list(P_char ch)
specializations.c:void do_specialize(P_char ch, char *argument, int cmd)
specs.dragonnia.c:void do_mobdisarm(P_char ch, char *argument, int cmd)
specs.eth2.c:void do_assist_core(P_char ch, P_char victim);
specs.library.c:void do_proclibObj(P_char ch, char *argument)
specs.library.c:void do_proclibMob(P_char ch, char *argument)
specs.library.c:void do_proclibRoom(P_char ch, char *argument)
specs.library.c:void do_proclib(P_char ch, char *argument, int cmd)
specs.zion.c:void do_dispator_remove(P_char ch)
sql.c:void do_sql(P_char ch, char *argument, int cmd)
sql.c:void do_sql(P_char ch, char *argument, int cmd)
statistics.c:void do_statistic(P_char ch, char *argument, int val)
testcmd.c:void do_test_room(P_char ch, char *arg, int cmd)
testcmd.c:void do_test_writemap(P_char ch, char *arg, int cmd)
testcmd.c:void do_test_add_epic_skillpoint(P_char ch, const char *charname)
testcmd.c:void do_test(P_char ch, char *arg, int cmd)
tether.c:void do_tether( P_char ch, char *argument, int cmd )
track.c:void do_track_not_in_use(P_char ch, char *arg, int cmd)
track.c:void do_track(P_char ch, char *arg, int cmd) //do_track_not_in_use
tradeskill.c:void do_forge(P_char ch, char *argument, int cmd)
tradeskill.c:void do_fish(P_char ch, char*, int cmd)
tradeskill.c:void do_mine(P_char ch, char *arg, int cmd)
tradeskill.c:void do_bandage(P_char ch, char *arg, int cmd)
tradeskill.c:void do_salvation(P_char ch, char *arg, int cmd)
tradeskill.c:void do_drandebug(P_char ch, char *arg, int cmd)
tradeskill.c:void do_refine(P_char ch, char *arg, int cmd)
tradeskill.c:void do_dice(P_char ch, char *arg, int cmd)
trap.c:void do_trapremove(P_char ch, char *argument, int cmd)
trap.c:void do_trapstat(P_char ch, char *argument, int cmd)
trap.c:void do_traplist(P_char ch, char *argument, int cmd)
trap.c:void do_trapset(P_char ch, char *argument, int cmd)
utility.c:void do_introduce(P_char ch, char *arg, int level)
utility.c:void do_testdesc(P_char ch, char *arg, int level)
