"""Which track a FOCUS message selects, without Live: the song's tracks are anything with Live's
names (name, is_grouped, group_track, fold_state), so tests/sf_e16_test.py runs it on its own."""

# The FOCUS message, as controllers/e16/layout.py has it: CC 119 on channel 16 (0-based here: 15).
FOCUS_CHANNEL = 15
FOCUS_CC = 119


def numbered(tracks, n):
    """The first track whose name's first word is the number n ("6 BASS" for 6, not "16 X"), or None."""
    want = str(n)
    for track in tracks:
        if track.name.split(" ", 1)[0] == want:
            return track
    return None


def select(song, n):
    """Selects track n in Live; returns it (None: no track has that number, nothing changes). A track
    inside a folded group that Live refuses to select gets its groups unfolded, then selected."""
    track = numbered(song.tracks, n) if n > 0 else None
    if track is None:
        return None
    try:
        song.view.selected_track = track
    except RuntimeError:
        group = track
        while group.is_grouped:
            group = group.group_track
            group.fold_state = False
        song.view.selected_track = track
    return track
