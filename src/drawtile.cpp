// --- drawing screen ---

namespace noteye {

struct TransCache : Tile {
  // WARNING: image in the cache is NOT indexed!
  // and thus it has to be removed together with TransCache
  smartptr<TileImage> orig, cache;
  int cachechg;
  int tx, ty, txy, tyx;
  int hash() const {
    return (tohash(orig) ^ (tx * 13157) ^ (ty * 71) ^ (txy * 5131) ^ (tyx * 61901)) & HASHMAX;
    }
  ~TransCache();
  };

bool eq(const TransCache& t1, const TransCache& t2) {
  return t1.orig == t2.orig && 
    t1.tx == t2.tx && t1.ty == t2.ty &&
    t1.txy == t2.txy && t1.tyx == t2.tyx;
  }

#ifdef SDL2
#define SDL_SRCCOLORKEY SDL_TRUE
#endif

void blitImage(Image *dest, int x, int y, TileImage *TI) {

  provideBoundingBox(TI);
  if(TI->bx <= TI->tx || TI->by <= TI->ty) return;

  SDL_SetColorKey(TI->i->s, SDL_SRCCOLORKEY, TI->trans);
  SDL_Rect srcrect;
  TI->i->setLock(false); dest->setLock(false);
  srcrect.x = TI->ox+TI->tx; srcrect.y = TI->oy+TI->ty;
  srcrect.w = TI->bx-TI->tx; srcrect.h = TI->by-TI->ty;
  SDL_Rect destrect;
  destrect.x = x+TI->tx; destrect.y = y+TI->ty;

#ifdef SDL2
  SDL_SetSurfaceBlendMode(TI->i->s, TI->trans == transAlpha ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);

#else    
  if(TI->trans == transAlpha) {
    SDL_SetAlpha(TI->i->s, SDL_SRCALPHA | SDL_RLEACCEL, 128);
    }
  else
    SDL_SetAlpha(TI->i->s, 0, 0);
  
#endif

  // SDL_BlitSurface(TI->i->s, &srcrect, dest->s, &destrect);
  
  if(TI->trans == transAlpha) {
    for(int y=0; y<srcrect.h; y++)
    for(int x=0; x<srcrect.w; x++)
      alphablend(qpixel(dest->s, destrect.x+x, destrect.y+y), qpixel(TI->i->s, srcrect.x+x, srcrect.y+y));
    }
  else {
    for(int y=0; y<srcrect.h; y++)
    for(int x=0; x<srcrect.w; x++)
      if((qpixel(TI->i->s, srcrect.x+x, srcrect.y+y) & 0xFFFFFF) != (TI->trans & 0xFFFFFF))
        qpixel(dest->s, destrect.x+x, destrect.y+y) = qpixel(TI->i->s, srcrect.x+x, srcrect.y+y);
    }

  }

int pos(int x) { return x>0?x:0; }
int neg(int x) { return x<0?x:0; }

int aaTable[5];

void aaInit() {
  for(int i=0; i<5; i++) aaTable[i] = 0;
  }
  
void aaAdd(noteyecolor pix) {
  int alpha = part(pix,3);
  for(int i=0; i<3; i++) aaTable[i] += part(pix,i) * alpha;
  aaTable[3] += alpha; aaTable[4]++;
  }

void aaPrintf() {
  printf("%d,%d,%d,%d,%d\n", aaTable[0], aaTable[1], aaTable[2], aaTable[3], aaTable[4]);
  }

noteyecolor aaResult() {
  if(aaTable[3] == 0) return 0;
  noteyecolor pix;
  part(pix,0) = aaTable[0] / aaTable[3];
  part(pix,1) = aaTable[1] / aaTable[3];
  part(pix,2) = aaTable[2] / aaTable[3];
  part(pix,3) = aaTable[3] / aaTable[4];
  return pix;
  }

#define AADD aaAdd(qpixel(TI->i->s, cx, cy) | (TI->trans==transNone?0xFF000000:0));

void scaleImage(Image *dest, drawmatrix &M, TileImage *TI, bool cache) {

  if(M.txy == 0 && M.tyx == 0 && (TI->trans == transAlpha || TI->trans == transNone) &&
    TI->sx >= M.tx && TI->sy >= M.ty) {
    // only transAlpha and transNone are antialiased currently
    TI->i->setLock(true); dest->setLock(true);
    int otx = M.tx, oty = M.ty, tox = TI->ox, toy = TI->oy, ox = M.x, oy = M.y;
    if(otx<0) { ox += otx; otx = -otx; tox += TI->sx-1; }
    if(oty<0) { oy += oty; oty = -oty; toy += TI->sy-1; }
    int cy1=toy, cy2=toy, cx1, cx2;
    for(int ay=0; ay<oty; ay++) {
      cy1=cy2, cy2 = toy+TI->sy*(ay+1)/M.ty;
      cx1=cx2=tox;
      for(int ax=0; ax<otx; ax++) {
        cx1=cx2, cx2 = tox+TI->sx*(ax+1)/M.tx;
        aaInit();
        for(int cy=cy1; cy<cy2; cy++) {
          for(int cx=cx1; cx<cx2; cx++) AADD
          for(int cx=cx1; cx>cx2; cx--) AADD
          if(cx1==cx2) { int cx=cx1; AADD }
          }
        for(int cy=cy1; cy>cy2; cy--) {
          for(int cx=cx1; cx<cx2; cx++) AADD
          for(int cx=cx1; cx>cx2; cx--) AADD
          if(cx1==cx2) { int cx=cx1; AADD }
          }
        if(cy1==cy2) {
          int cy=cy1;
          for(int cx=cx1; cx<cx2; cx++) AADD
          for(int cx=cx1; cx>cx2; cx--) AADD
          if(cx1==cx2) { int cx=cx1; AADD }
          }
          
        if(aaTable[3])
          alphablendc(qpixel(dest->s, ox+ax, oy+ay), aaResult(), cache);
        else if(cache)
          qpixel(dest->s, ox+ax, oy+ay) = 0;
        }
      }
    return;
    }

  if(M.txy == 0 && M.tyx == 0) {
    TI->i->setLock(true); dest->setLock(true);
    int otx = M.tx, oty = M.ty, tox = TI->ox, toy = TI->oy, ox = M.x, oy = M.y;
    if(otx<0) { ox += otx; otx = -otx; tox += TI->sx-1; }
    if(oty<0) { oy += oty; oty = -oty; toy += TI->sy-1; }
    for(int ay=0; ay<oty; ay++) {
      int cy = TI->sy*ay/M.ty;
      for(int ax=0; ax<otx; ax++) {
        int cx = TI->sx*ax/M.tx;
        int pix = qpixel(TI->i->s, tox + cx, toy + cy);
        if(TI->trans == transAlpha) {
          alphablendc(qpixel(dest->s, ox+ax, oy+ay), pix, cache);
          }
        else if(cache || !istrans(pix, TI->trans)) 
          qpixel(dest->s, ox+ax, oy+ay) = pix;
        }
      }
    return;
    }

#define AAF 2
  if(TI->trans == transAlpha) {
    // only transAlpha is antialiased currently
    TI->i->setLock(true); dest->setLock(true);
    int xmin = neg(M.tx) + neg(M.tyx);
    int ymin = neg(M.ty) + neg(M.txy);
    int xmax = pos(M.tx) + pos(M.tyx);
    int ymax = pos(M.ty) + pos(M.txy);
    int det = M.tx*M.ty-M.txy*M.tyx;
    int rev = abs(det) * AAF;
    for(int ay=ymin; ay<ymax; ay++)
    for(int ax=xmin; ax<xmax; ax++) {
      aaInit();
      for(int ux=ax*AAF; ux<ax*AAF+AAF; ux++)
      for(int uy=ay*AAF; uy<ay*AAF+AAF; uy++) {
        int cx = (TI->sx * (ux*M.ty-uy*M.tyx));
        int cy = (TI->sy * (uy*M.tx-ux*M.txy));
        if(det<0) cx = -cx, cy = -cy;
        if(M.ty < 0 || M.txy < 0) cx--;
        if(M.tx < 0 || M.tyx < 0) cy--;
        // if(M.ty<0 || M.tyx<0) cx--; if(M.tx<0 || M.txy<0) cy--;
        if(cx<0 || cy<0 || cx >= rev*TI->sx || cy >= rev*TI->sy) continue;
        cx /= rev; cy /= rev;
        int pix = qpixel(TI->i->s, TI->ox+cx, TI->oy + cy);
        aaAdd(pix);
        }
      alphablendc(qpixel(dest->s, M.x+ax, M.y+ay), aaResult(), cache);
      }
    return;
    }

  if(1) {
    TI->i->setLock(true); dest->setLock(true);
    int xmin = neg(M.tx) + neg(M.tyx);
    int ymin = neg(M.ty) + neg(M.txy);
    int xmax = pos(M.tx) + pos(M.tyx);
    int ymax = pos(M.ty) + pos(M.txy);
    int det = M.tx*M.ty-M.txy*M.tyx;
    int rev = abs(det);
    for(int ay=ymin; ay<ymax; ay++)
    for(int ax=xmin; ax<xmax; ax++) {
      int cx = (TI->sx * (ax*M.ty-ay*M.tyx));
      int cy = (TI->sy * (ay*M.tx-ax*M.txy));
      if(det<0) cx = -cx, cy = -cy;
      if(cx<0 || cy<0 || cx >= rev*TI->sx || cy >= rev*TI->sy) continue;

      cx /= rev; cy /= rev;
      int pix = qpixel(TI->i->s, TI->ox+cx, TI->oy + cy);
      if(TI->trans == transAlpha) {
        alphablendc(qpixel(dest->s, M.x+ax, M.y+ay), pix, cache);
        }
      else if(cache || !istrans(pix, TI->trans))
        qpixel(dest->s, M.x+ax, M.y+ay) = pix;
      }
    return;
    }
  }

void TileImage::draw(Image *dest, drawmatrix& M) { 
//  printf("trans = %08x\n", TI->trans);
  #ifdef OPENGL
  if(useGL(dest)) {
    drawTileImageGL(useGL(dest), M, this);
    return;
    }
  if(useSDL(dest) && matrixIsStraight(M)) {
    drawTileImageSDL(useSDL(dest), M, this);
    return;
    }
  #endif

  #ifdef USEBLITS
  if(M.tx == sx && M.ty == sy && M.txy == 0 && M.tyx == 0) {
    blitImage(dest, M.x, M.y, this);
    return;
    }
  #endif

  if((abs(M.tx)+abs(M.tyx))*(abs(M.ty)+abs(M.txy)) <= 128*128 || useSDL(dest)) {  
    TransCache tc;
    tc.orig = this;
    tc.tx = M.tx; tc.ty = M.ty; tc.txy = M.txy; tc.tyx = M.tyx;
    tc.cache = NULL; Tile *rt = registerTile(tc);
    Get(TransCache, TC, rt);

    if(!TC->cache) {
      caches.push_back(TC);
      int col = trans == transAlpha ? 0 : trans;
      int atx = abs(M.tx)+abs(M.tyx);
      int aty = abs(M.ty)+abs(M.txy);
      Image *I = new Image(atx, aty, col);
      totalimagecache += atx * aty;
      TC->cache = new TileImage(atx, aty);
      TC->cache->i = I;
      TC->cache->trans = trans;
      TC->cachechg = -5;
      registerObject(TC->cache);
      }
    
    int nx = neg(M.tx)+neg(M.tyx);
    int ny = neg(M.ty)+neg(M.txy);
    
    if(TC->cachechg != i->changes) {
      drawmatrix M2 = M;
      M2.x = -nx;
      M2.y = -ny;
      scaleImage(TC->cache->i, M2, this, true);
      TC->cachechg = i->changes;
      }
    
    if(useSDL(dest))
      blitImageSDL(useSDL(dest), M.x+nx, M.y+ny, TC->cache);
    else
      blitImage(dest, M.x+nx, M.y+ny, TC->cache);
    return;
    }
  
  scaleImage(dest, M, this, false);
  }

TileImage::~TileImage() {
  all_images.erase(this);
#ifdef OPENGL
  deleteTextureGL(this);
#endif
  deleteTextureSDL(this);
  }

TransCache::~TransCache() {
  if(cache) {
    totalimagecache -= cache->sx * cache->sy;
    }
  }

Image *pcache; int pcachex = 0;

TileFill::~TileFill() {
  if(cache) totalimagecache -= 1024;
  }

TileImage *getFillCache(TileFill *TF) {
  if(!TF->cache) {
    if(pcachex == 1024 || !pcache)
      pcache = new Image(1024, 1), pcachex = 0, totalimagecache += 1024;
    noteyecolor& pix(qpixel(pcache->s, pcachex, 0));
    pix = TF->color;
    TileImage *ti = new TileImage(1, 1);
    ti->ox = pcachex;
    ti->i = pcache;
    ti->trans = transNone;
    if(TF->alpha != 0xFFFFFF) {
      TF->alpha = transAlpha;
      part(pix, 3) = (part(TF->alpha, 0) + part(TF->alpha, 1) + part(TF->alpha,2)+1) / 3;
      }
    registerObject(ti);
    TF->cache = ti;
    pcachex++;
    }
  return TF->cache;
  }

void TileFill::draw(Image *dest, drawmatrix& M) {
  
  #ifdef OPENGL
  if(useGL(dest)) {
    drawFillGL(useGL(dest), M, this);
    }
  else if(useSDL(dest) && matrixIsStraight(M)) {
    drawFillSDL(useSDL(dest), M, this);
    }

  else
  #endif

  if(M.txy || M.tyx) {
    getFillCache(this)->draw(dest, M);
    }
  
  else if(alpha == 0xffffff) {
    SDL_Rect dstrect;
    dstrect.x = M.x; dstrect.y = M.y; dstrect.w = M.tx; dstrect.h = M.ty;
    SDL_FillRect(dest->s, &dstrect, color);
    }
  else if(alpha == 0x808080) {
    dest->setLock(true);
    for(int ax=0; ax<M.tx; ax++) for(int ay=0; ay<M.ty; ay++) 
      mixcolor(qpixel(dest->s, M.x+ax, M.y+ay), color);
    }
  else {
    dest->setLock(true);
    for(int ax=0; ax<M.tx; ax++) for(int ay=0; ay<M.ty; ay++) 
      mixcolorAt(qpixel(dest->s, M.x+ax, M.y+ay), color, alpha);
    }
  }

int roundint(long double d) {
  if(d>0) d += .5;
  else d -= .5;
  return int(d);
  }

void drawTile(Image *dest, drawmatrix& M, Tile *c) {
  if(c) c->draw(dest, M);
  }

void TileMerge::draw(Image *dest, drawmatrix& M) { t1->draw(dest, M); t2->draw(dest, M); }

void TileTransform::draw(Image *dest, drawmatrix& M) {
  drawmatrix M2;
  M2.x  = M.x + roundint(M.tx * dx + M.tyx * dy + M.tzx * dz);
  M2.y  = M.y + roundint(M.ty * dy + M.txy * dx + M.tzy * dz);

  double C = sx * +cos(rot*M_PI/180);
  double S = sx * -sin(rot*M_PI/180);

  M2.ty  = roundint(M.ty  * sy);
  M2.tyx = roundint(M.tyx * sy);

  M2.tx  = roundint(M.tx  * C - M.tzx * S);
  M2.txy = roundint(M.txy * C - M.tzy * S);
  M2.tzx = roundint(M.tx  * S + M.tzx * C);
  M2.tzy = roundint(M.txy * S + M.tzy * C);
  
  drawTile(dest, M2, t1);
  }

void TileFreeform::draw(Image *dest, drawmatrix& M) {
  drawmatrix M2;
  auto& D = par->d;
  M2.x = int(M.x * D[0][0] + M.tx  * D[0][1] + M.tyx * D[0][2] + M.tzx * D[0][3]);
  M2.y = int(M.y * D[0][0] + M.txy * D[0][1] + M.ty  * D[0][2] + M.tzy * D[0][3]);
  M2.tx  = int(M.tx  * D[1][1] + M.tyx * D[1][2] + M.tzx * D[1][3]);
  M2.txy = int(M.txy * D[1][1] + M.ty  * D[1][2] + M.tzy * D[1][3]);
  M2.tyx = int(M.tx  * D[2][1] + M.tyx * D[2][2] + M.tzx * D[2][3]);
  M2.ty  = int(M.txy * D[2][1] + M.ty  * D[2][2] + M.tzy * D[2][3]);
  M2.tzx = int(M.tx  * D[3][1] + M.tyx * D[3][2] + M.tzx * D[3][3]);
  M2.tzy = int(M.txy * D[3][1] + M.ty  * D[3][2] + M.tzy * D[3][3]);
  drawTile(dest, M2, t1);
  }

}
