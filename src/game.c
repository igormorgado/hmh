
internal int
RenderWeirdGradient(struct game_offscreen_buffer *buffer, i16 blueoffset, i16 greenoffset)
{

    // TODO: ASSERT WINDOW NOT NULL
    // TODO: ASSERT 0 < BLUEOFFSET GREEN OFFSET < 255
    u8 *row = (u8 *)buffer->memory;
    for(int y = 0; y < buffer->height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < buffer->width; ++x)
        {
            u8 blue = (x + blueoffset);
            u8 green = (y + greenoffset);
            *pixel++ = ((red << 16)  |
                        (green << 8) |
                        (blue << 0));
        }
        row += buffer->pitch;
    }
    return RETURN_SUCCESS;
}

internal void
GameUpdateAndRender(struct game_offscreen_buffer *buffer, i16 blueoffset, i16 greenoffset)
{
    RenderWeirdGradient(buffer, blueoffset, greenoffset);
}
