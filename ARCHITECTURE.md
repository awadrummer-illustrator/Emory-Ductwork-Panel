# Emory Illustrator Architecture

## Goal

Build an Illustrator version of the Emory ductwork workflow that keeps the fast, reliable Magic tooling where it still applies and moves Emory-specific duct geometry into the native C++ plugin.

## Shared From Magic

- orthogonalizing
- carve/gap tools
- register placement
- thermostat placement
- layer utilities
- selection helpers
- CEP panel shell
- Illustrator script-message plumbing

## New Emory Engine

The new native geometry engine needs to own:

- centerline-to-duct body expansion
- straight segment bodies
- elbows and angled connectors
- segment width metadata
- cascade direction and width propagation
- in-place regeneration after centerline edits
- Emory fill/stroke styling

## Recommended Port Order

1. Add an Emory document model in C++ for selected centerlines, segments, junctions, color, and per-segment width.
2. Port straight body generation for a single open path.
3. Port connector generation for 90-degree and non-90-degree elbows.
4. Add per-segment width overrides and cascade propagation.
5. Add regroup/rebuild logic so rerunning generation updates the existing run instead of duplicating it.
6. Wire panel controls for width, start segment, joint style, and color presets.

## Guardrail

Do not reuse the old hidden Magic `process-emory` geometry path as the production engine. It is only historical reference.
