#include <stdio.h>
#include <stdlib.h>
#include <mjolnir/conf.h>
#include <mjolnir/mjol.h>
#include <mjolnir/scr.h>

extern long mjolgetopt(struct mjolgame *game, int argc, char *argv[]);
extern void mjolinitscr(struct mjolgame *game);
extern void mjolgendng(struct mjolgame *game);
extern void mjolinitcmd(void);
extern void mjoldocmd(struct mjolgame *game, int ch);

char          mjolgamename[] = "mjolnir";
volatile long mjolquit;

void
mjolgameintro(void)
{
    int ch;

    printf("\n");
    printf("Legend has it that many moons ago, Fenris, the eldest child of Loki and\n");
    printf("Angrboda, broke free from his chain, Pleignir, and stole Mjolnir from Thor.\n");
    printf("Fenris is believed to have hidden Mjolnir, Thor's hammer, into a dungeon.\n");
    printf("\n");
    printf("It is said that Thor shall ascend the one to return Mjolnir to a deity for\n");
    printf("saving the world from the forces of evil creating chaos with lightnings and\n");
    printf("thunder. Clairvoyants say evil forces are already on the hunt for Mjolnir.\n");
    printf("\n");
    printf("Armed with nothing more than a few food rations, your pet dog, a +1 blessed\n");
    printf("armor, a +3 neutral sword, and a piece of Pleignir, you enter the Dungeon.\n");
    printf("\n");
    printf("Good luck, adventurer, and beware of Fenris!\n");
    printf("\n");
    printf("Press a key to continue...\n");
    ch = getchar();

    return;
}

void
mjolinitgame(struct mjolgame *game, int argc, char *argv[])
{
    struct mjolgame *data = calloc(1, sizeof(struct mjolgame *));
    long             x;
    
    if (!data) {
        fprintf(stderr, "failed to allocate game data\n");

        exit(1);
    }
    game->data.name = mjolgamename;
    game->nicks = calloc(1, sizeof(struct dngchar *));
    mjolgetopt(game, argc, argv);
    if (!game->nicks[0]) {
        game->nicks[0] = MJOL_DEF_NICK;
    }
    if (!game->scrtype) {
#if (MJOL_TTY)
        game->scrtype = MJOL_SCR_TTY;
#elif (MJOL_VGA_TEXT)
        game->scrtype = MJOL_SCR_VGA_TEXT;
#endif
    }
#if 0
    if (!game->scrtype) {
        fprintf(stderr, "no supported screen type found\n");
        
        exit(1);
    }
#endif
    mjolgameintro();
    mjolinitscr(game);
    if (!game->nlvl) {
        game->nlvl = MJOL_DEF_NLVL;
    }
    if (!game->width) {
        game->width = MJOL_DEF_WIDTH;
    }
    if (!game->height) {
        game->height = MJOL_DEF_HEIGHT;
    }
    game->objtab = calloc(game->width, sizeof(struct mjolobj *));
    for (x = 0 ; x < game->width ; x++) {
        game->objtab[x] = calloc(game->height, sizeof(struct mjolobj *));
    }
    mjolinitscr(game);
    mjolinitcmd();
    
    return;
}

void
mjolheartbeat(void)
{
    ;
}

void
mjolgameloop(struct mjolgame *game)
{
    int (*getkbd)(void) = game->scr->getch;
    int   ch;

    do {
        ch = getkbd();
        mjoldocmd(game, ch);
        mjolheartbeat();
    } while (!mjolquit);
}

