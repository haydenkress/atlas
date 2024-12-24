// server.js
const express = require('express');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const { createClient } = require('@deepgram/sdk');
const { OpenAI } = require('openai');
const { ElevenLabsClient } = require('elevenlabs');  // We'll handle streaming or returning full file

const { Readable } = require('stream');

const app = express();
const port = 3000;

// Initialize API clients
const deepgram = createClient(process.env.DEEPGRAM_API_KEY);
const openai = new OpenAI({ apiKey: process.env.OPENAI_API_KEY });
const elevenLabs = new ElevenLabsClient({ apiKey: process.env.ELEVEN_LABS_API_KEY });

// -------------------------
// 1. Route to handle audio
// -------------------------
app.post('/upload', async (req, res) => {
  try {
    // Collect raw audio data from the request
    const chunks = [];
    req.on('data', (chunk) => chunks.push(chunk));
    
    req.on('end', async () => {
      const audioBuffer = Buffer.concat(chunks);

      // 1) Transcribe with Deepgram
      const transcript = await transcribeAudio(audioBuffer);
      console.log('Deepgram Transcript:', transcript);

      // 2) Send transcript to GPT-4
      const textResponse = await getOpenAIResponse(transcript);
      console.log('OpenAI GPT-4 Response:', textResponse);

      // 3) Convert GPT-4 response to speech via ElevenLabs
      const ttsAudio = await textToSpeech(textResponse);
      // ttsAudio is a Buffer of audio bytes (MP3, WAV, etc. depending on your settings)

      // 4) Send back the TTS audio to the ESP32
      // For a raw audio buffer, set appropriate headers
      res.setHeader('Content-Type', 'audio/mpeg'); // or audio/wav
      res.send(ttsAudio);
    });
  } catch (error) {
    console.error('Error processing request:', error);
    res.status(500).json({ error: 'Internal server error' });
  }
});

// ---------------
// Deepgram STT
// ---------------
async function transcribeAudio(audioBuffer) {
  try {
    // Using the new @deepgram/sdk approach
    // If you’re using the older approach, adapt accordingly.
    const { results } = await deepgram.listen.prerecorded.preRecordedFile(
      { buffer: audioBuffer, mimetype: 'audio/wav' },
      { model: 'nova-2' } // or 'nova-2' if you have access
    );
    
    const transcript = results?.channels[0]?.alternatives[0]?.transcript || '';
    return transcript;
  } catch (error) {
    console.error('Error transcribing with Deepgram:', error);
    throw error;
  }
}

// ---------------------
// OpenAI Chat (GPT-4)
// ---------------------
async function getOpenAIResponse(userPrompt) {
  try {
    const completion = await openai.chat.completions.create({
      model: 'gpt-4',
      messages: [
        {
          role: 'system',
          content: 'You are ATLAS, a helpful AI assistant with an attitude and demeanor like JARVIS from Iron Man. Keep responses concise and natural.'
        },
        { role: 'user', content: userPrompt }
      ],
      temperature: 0.5,
      max_tokens: 100
    });

    const textResponse = completion.choices[0].message.content.trim();
    return textResponse;
  } catch (error) {
    console.error('Error with OpenAI GPT-4:', error);
    throw error;
  }
}

// ----------------------------------------------
// ElevenLabs TTS (returns a Buffer of audio data)
// ----------------------------------------------
async function textToSpeech(text) {
  try {
    // Adjust voiceId, modelId, and outputFormat as desired
    const voiceId = 'JBFqnCBsd6RMkjVDRZzb'; // Your chosen voice
    const modelId = 'eleven_multilingual_v2';

    // This returns a readable stream
    const audioStream = await elevenLabs.textToSpeech.stream(voiceId, {
      text,
      modelId,
      // For WAV: outputFormat: 'pcm_44100' or 'pcm_48000'
      // For MP3: outputFormat: 'mp3_44100_128'
      outputFormat: 'mp3_44100_128'
    });

    // Convert that stream to a Buffer
    const chunks = [];
    for await (const chunk of audioStream) {
      chunks.push(chunk);
    }
    return Buffer.concat(chunks);
  } catch (error) {
    console.error('Error converting text to speech with ElevenLabs:', error);
    throw error;
  }
}

// -----------------------
// Start the Express App
// -----------------------
app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});
app.get('/', (req, res) => {
    res.send('Welcome to the ATLAS server!');
  });
