#include "e_mod_main.h"
#include "echievements.x"

#define ECB(NAME) void echievement_init_cb_##NAME(Echievement *ec)
#define ECH_EH_NAME(NAME) echievement_##NAME##_handler_cb
#define ECH_EH(NAME, TYPE) static Eina_Bool echievement_##NAME##_handler_cb(Echievement *ec, int type EINA_UNUSED, TYPE *ev)

static Ecore_Idler *_ech_idler = NULL;

/* helper for init */
static void
_ech_list_add(Etrophy_Trophy *et)
{
   Echievement *ec;

   ec = E_NEW(Echievement, 1);
   ec->trophy = et;
   ec->id = eina_hash_population(mod->trophies);
   eina_hash_add(mod->trophies, &ec->id, ec);
   mod->trophies_list = (Echievement*)eina_inlist_append(EINA_INLIST_GET(mod->trophies_list), EINA_INLIST_GET(ec));
   Echievement_Callbacks[ec->id](ec);
}

static void
_ech_add(Echievement_Id id)
{
   Etrophy_Trophy *et;

   et = etrophy_trophy_new(Echievement_Strings[id], Echievement_Descs[id], Echievement_Hide_State[id], 0);
   _ech_list_add(et);
}

static void
_ech_free(Echievement *ec)
{
   /* trophy does NOT get freed here!!! */
   E_FREE_LIST(ec->handlers, ecore_event_handler_del);
   free(ec);
}

/* returns trophy only if it has not yet been earned */
static Etrophy_Trophy *
_ech_lookup(Echievement_Id id)
{
   Echievement *ec;

   ec = eina_hash_find(mod->trophies, &id);
   EINA_SAFETY_ON_NULL_RETURN_VAL(ec, NULL);
   if (!etrophy_trophy_earned_get(ec->trophy)) return ec->trophy;
   INF("TROPHY PREVIOUSLY EARNED: %s - %s", etrophy_trophy_name_get(ec->trophy), etrophy_trophy_description_get(ec->trophy));
   return NULL;
}

/* call whenever new echievement is earned, ec may be NULL */
static void
_ech_hook(Echievement_Id id, Echievement *ec)
{
   Etrophy_Trophy *et;

   if (ec)
     et = ec->trophy;
   else
     et = _ech_lookup(id);
   if (!et) return;
   etrophy_trophy_counter_increment(et, Echievement_Goals[id]);
   INF("TROPHY EARNED: %s - %s", etrophy_trophy_name_get(et), etrophy_trophy_description_get(et));
   /* FIXME: popup here */
}

static Eina_Bool
_ech_init_check_idler(void *d EINA_UNUSED)
{
   Echievement *ec;
   unsigned int count = 0;
   Eina_Inlist *l;

   l = mod->itr ?: EINA_INLIST_GET(mod->trophies_list);
   EINA_INLIST_FOREACH(l, ec)
     {
        if (count > 20)
          {
             mod->itr = EINA_INLIST_GET(ec);
             return EINA_TRUE;
          }
        if (etrophy_trophy_earned_get(ec->trophy)) continue;
        count++;
     }
   _ech_idler = ecore_idler_del(_ech_idler);
   return EINA_FALSE;
}

static Eina_Bool
_ech_init_add_idler(void *d EINA_UNUSED)
{
   Echievement_Id id = 0;
   unsigned int count = 0;

   if (mod->trophies_list)
     {
        Echievement *ec;

        ec = EINA_INLIST_CONTAINER_GET(EINA_INLIST_GET(mod->trophies_list)->last, Echievement);
        id = ec->id + 1;
     }
   for (; id < ECHIEVEMENT_LAST; count++, id++)
     {
        if (count > 20) return EINA_TRUE;
        _ech_add(id);
     }
   ecore_idler_del(_ech_idler);
   _ech_idler = ecore_idler_add(_ech_init_check_idler, NULL);
   return EINA_FALSE;
}

ECH_EH(SHELF_POSITIONS, E_Event_Shelf EINA_UNUSED)
{
   if (eina_list_count(e_shelf_list()) != Echievement_Goals[ec->id]) return ECORE_CALLBACK_RENEW;
   _ech_hook(ec->id, ec);
   E_FREE_LIST(ec->handlers, ecore_event_handler_del);
   return ECORE_CALLBACK_RENEW;
}

ECB(SHELF_POSITIONS)
{
   if (eina_list_count(e_shelf_list()) == Echievement_Goals[ec->id])
     {
        /* number of shelves equals goal, grant trophy */
        _ech_hook(ec->id, ec);
        return;
     }
   E_LIST_HANDLER_APPEND(ec->handlers, E_EVENT_SHELF_ADD, ECH_EH_NAME(SHELF_POSITIONS), ec);
}

void
ech_init(void)
{
   /* FIXME: delay this */
   mod->trophies = eina_hash_int32_new((Eina_Free_Cb)_ech_free);
   if (ech_config->gs)
     {
        E_LIST_FOREACH(etrophy_gamescore_trophies_list_get(ech_config->gs), _ech_list_add);
        _ech_idler = ecore_idler_add(_ech_init_check_idler, NULL);
     }
   else
     _ech_idler = ecore_idler_add(_ech_init_add_idler, NULL);
}

void
ech_shutdown(void)
{
   eina_hash_free(mod->trophies);
   mod->trophies = NULL;
   mod->trophies_list = NULL;
   mod->itr = NULL;
   if (_ech_idler) _ech_idler = ecore_idler_del(_ech_idler);
}