/*
  Compute 3D index of the cell containing given position
*/
uvec3 getCell3DIndexFromPos(vec4 pos)
{
  // Moving particles in [0 - 2 * ABS_WALL_X] to have coords matching with cellIndices
  const vec4 normalizedPos = clamp(pos, MIN_BOUNDS, MAX_BOUNDS) + MAX_BOUNDS;

  const uvec3 cell3DIndex = uvec3(floor(normalizedPos.xyz / GRID_CELL_SIZE_XYZ));

  return cell3DIndex;
}

/*
  Compute 1D index of the cell containing given position
*/
uint getCell1DIndexFromPos(vec4 pos)
{
  const uvec3 cell3DIndex = getCell3DIndexFromPos(pos);

  const uint cell1DIndex = cell3DIndex.x * GRID_RES_Z * GRID_RES_Y
      + cell3DIndex.y * GRID_RES_Z
      + cell3DIndex.z;

  return cell1DIndex;
}