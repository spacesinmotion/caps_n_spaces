#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <sndfile.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338
#endif
#define TWO_PI (2.0 * M_PI)

inline float minf(float a, float b) { return a < b ? a : b; }
inline float maxf(float a, float b) { return a > b ? a : b; }
inline float clampf(float a, float b, float c) { return maxf(a, minf(b, c)); }

#define SAMPLE_RATE 44100

float white_noise() { return 2.0f * ((float)rand() / (float)RAND_MAX) - 1.0f; }

typedef struct {
  float left, right;
} Frame;

typedef struct {
  float amplitude, envDecay, frequency, freqDecay, noiseAmount, noiseDecay, limit, phase;
} Drum;

void Drum_start(Drum *bd) { bd->phase = 0.0f; }

Frame Drum_next(Drum *bd, float sampleRate, int f) {
  const float tone = sin(bd->phase) * (1.0f - bd->noiseAmount);
  const float v_envelope = exp(-bd->envDecay * f / sampleRate);
  const float n_envelope = exp(-bd->noiseDecay * f / sampleRate);
  const float na = bd->noiseAmount * white_noise() * n_envelope;
  const float nb = bd->noiseAmount * white_noise() * n_envelope;
  const float n = 0.7f * (na + nb);

  const float currentFrequency = bd->frequency * exp(-bd->freqDecay * f / sampleRate);
  bd->phase += TWO_PI * currentFrequency / sampleRate;
  if (bd->phase >= TWO_PI)
    bd->phase -= TWO_PI;

  return (Frame){
      clampf(-bd->limit, bd->limit, bd->amplitude * v_envelope * (tone + n + na * 0.3f)),
      clampf(-bd->limit, bd->limit, bd->amplitude * v_envelope * (tone + n + nb * 0.3f)),
  };
}

void clear(float *buffer, int bufferSize) {
  for (int i = 0; i < bufferSize; i++)
    buffer[i] = 0.0f;
}

typedef struct {
  SNDFILE *file;
  float *buffer;
  int bufferSize;
  int in_buffer;
} SongWriter;

SongWriter SongWriter_create(const char *file, int numChannels, int bufferSize) {
  SF_INFO sfinfo = (SF_INFO){};
  sfinfo.samplerate = SAMPLE_RATE;
  sfinfo.channels = numChannels;
  sfinfo.format = (SF_FORMAT_WAV | SF_FORMAT_PCM_16);

  SNDFILE *f = sf_open(file, SFM_WRITE, &sfinfo);

  return (SongWriter){f, malloc(numChannels * bufferSize * sizeof(float)), numChannels * bufferSize, 0};
}

void SongWriter_flush(SongWriter *w) {
  if (w->in_buffer <= 0)
    return;
  if (sf_write_float(w->file, w->buffer, w->in_buffer) != w->in_buffer)
    puts(sf_strerror(w->file));
  w->in_buffer = 0;
}

void SongWriter_add_frame(SongWriter *w, Frame f) {
  w->buffer[w->in_buffer] = f.left;
  w->buffer[w->in_buffer + 1] = f.right;
  w->in_buffer += 2;
  if (w->in_buffer == w->bufferSize)
    SongWriter_flush(w);
}

void SongWriter_dispose(SongWriter *w) {
  SongWriter_flush(w);
  free(w->buffer);
  w->bufferSize = 0;
  sf_close(w->file);
  w->file = NULL;
}

typedef struct {
  void (*start)(void *ud);
  Frame (*next)(void *ud, float sampleRate, int f);
} FrameBuilderDefinition;

FrameBuilderDefinition *FrameBuilder_drum = &(FrameBuilderDefinition){Drum_start, Drum_next};

typedef struct {
  FrameBuilderDefinition *tab;
  void *ud;
} FrameBuilder;

typedef struct {
  int frame;
  FrameBuilder fb;
} Sample;

typedef struct {
  Sample *s;
  int frame;
} Pattern;

Frame Pattern_next_frame(Pattern *p, int i) {
  if (i < p->s->frame)
    return (Frame){};
  if (i >= (p->s + 1)->frame) {
    p->s++;
    if (!p->s->fb.tab)
      return (Frame){};
    p->s->fb.tab->start(p->s->fb.ud);
    p->frame = 0;
  }
  return p->s->fb.tab->next(p->s->fb.ud, SAMPLE_RATE, p->frame++);
}

int main(void) {

  if (sizeof(float[2]) != sizeof(Frame))
    return EXIT_FAILURE;

  SongWriter w = SongWriter_create("../wav_play/test.wav", 2, 30000);
  // for (int i = 0; i < (int)(0.1 * SAMPLE_RATE); ++i)
  //   SongWriter_add_frame(&w, (Frame){});

  Drum bd = (Drum){
      .amplitude = 1.0,
      .envDecay = 15.0f,
      .frequency = 110.0f,
      .freqDecay = 28.0f,
      .noiseAmount = 0.1f,
      .noiseDecay = 28.0f,
      .limit = 0.5f,
  };
  Drum sd = (Drum){
      .amplitude = 0.6f,
      .envDecay = 15.0f,
      .frequency = 279.0f,
      .freqDecay = 45.0f,
      .noiseAmount = 0.5f,
      .noiseDecay = 10.1f,
      .limit = 0.4f,
  };
  Drum sA = (Drum){
      .amplitude = 0.6f,
      .envDecay = 15.0f,
      .frequency = 110.0f,
      .freqDecay = 0.0f,
      .noiseAmount = 0.01f,
      .noiseDecay = 10.1f,
      .limit = 0.4f,
  };
  Drum sG = (Drum){
      .amplitude = 0.6f,
      .envDecay = 15.0f,
      .frequency = 97.999f,
      .freqDecay = 0.f,
      .noiseAmount = 0.01f,
      .noiseDecay = 10.1f,
      .limit = 0.4f,
  };
  Drum sB = (Drum){
      .amplitude = 0.6f,
      .envDecay = 15.0f,
      .frequency = 123.471f,
      .freqDecay = 0.0f,
      .noiseAmount = 0.01f,
      .noiseDecay = 10.1f,
      .limit = 0.4f,
  };

  const int tick = (int)((double)SAMPLE_RATE * 60.0 / 134.0 / 4.0);

  Sample samples[] = {
      {0 * tick, {FrameBuilder_drum, &bd}},  {2 * tick, {FrameBuilder_drum, &sA}},
      {3 * tick, {FrameBuilder_drum, &bd}},  {4 * tick, {FrameBuilder_drum, &sd}},
      {6 * tick, {FrameBuilder_drum, &sA}},  {8 * tick, {FrameBuilder_drum, &bd}},
      {11 * tick, {FrameBuilder_drum, &sd}}, {16 * tick, {FrameBuilder_drum, &sG}},
      {18 * tick, {FrameBuilder_drum, &bd}}, {20 * tick, {FrameBuilder_drum, &sd}},
      {22 * tick, {FrameBuilder_drum, &sG}}, {24 * tick, {FrameBuilder_drum, &bd}},
      {27 * tick, {FrameBuilder_drum, &sd}}, {28 * tick, {FrameBuilder_drum, &sG}},
      {30 * tick, {FrameBuilder_drum, &sB}}, {32 * tick, {NULL, NULL}},
  };

  float x = 0.0;
  for (int k = 0; k < 8; ++k) {
    Pattern p = (Pattern){samples, 0};
    for (int i = 0; p.s && p.s->fb.ud && p.s->fb.ud; ++i) {
      Frame f = Pattern_next_frame(&p, i);
      SongWriter_add_frame(&w, f);

      x += 1.0f / (float)SAMPLE_RATE;
      sd.noiseDecay = 10.2f + 8.0f * sin(x * M_PI / 4.0f);
      bd.freqDecay = 28.5f - 18.0f * sin(x * M_PI / 4.0f);
      sA.limit = sG.limit = sB.limit = 0.3f + 0.1f * sin(x * M_PI / 8.0f);
    }
  }
  SongWriter_dispose(&w);
  return EXIT_SUCCESS;
}
