#include "test.h"
#include "../src/mixer.h"
#include "../src/virtual.h"

TEST(test_effect_delay)
{
	xmp_context opaque;
	struct context_data *ctx;
	struct player_data *p;
	struct mixer_voice *vi;
	int voc;

	opaque = xmp_create_context();
	ctx = (struct context_data *)opaque;
	p = &ctx->p;

 	create_simple_module(ctx);
	new_event(ctx, 0, 0, 0, 60, 2, 40, 0x0f, 0x02, 0, 0);
	new_event(ctx, 0, 1, 0, 61, 1,  0, 0x00, 0x00, 0, 0);
	new_event(ctx, 0, 2, 0, 62, 2,  0, 0x0e, 0xd0, 0, 0);

	xmp_player_start(opaque, 0, 44100, 0);

	/* Row 0 */
	xmp_player_frame(opaque);

	voc = map_virt_channel(p, 0);
	fail_unless(voc >= 0, "virtual map");
	vi = &p->virt.voice_array[voc];

	fail_unless(vi->note == 59, "row 0 frame 0");

	xmp_player_frame(opaque);
	fail_unless(vi->note == 59, "row 0 frame 1");

	/* Row 1 */
	xmp_player_frame(opaque);
	fail_unless(vi->note == 60, "row 1 frame 0");

	xmp_player_frame(opaque);
	fail_unless(vi->note == 60, "row 1 frame 1");

	/* Row 2: delay this frame */
	xmp_player_frame(opaque);
	fail_unless(vi->note == 60, "row 2 frame 0");

	/* note changes in the frame 1 */
	xmp_player_frame(opaque);
	fail_unless(vi->note == 61, "row 2 frame 1");
}
END_TEST
