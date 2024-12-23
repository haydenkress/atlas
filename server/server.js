// server.js
const express = require('express');
const multer = require('multer');
const fs = require('fs');
const path = require('path');
const { createClient } = require("@deepgram/sdk");
const { OpenAI } = require('openai');
const wav = require('wav');
const axios = require('axios');
require('dotenv').config();

// Start express server
const app = express();
const port = 3000;

// Initialize API clients
const deepgram = createClient(DEEPGRAM_API_KEY);
const openai = new OpenAI({
    apiKey: process.env.OPENAI_API_KEY
});

// Configure multer for handling file uploads
const storage = multer.memoryStorage();
const upload = multer({ storage: storage });

// Ensure directories exist
const recordingsDir = path.join(__dirname, 'recordings');
const responsesDir = path.join(__dirname, 'responses');
if (!fs.existsSync(recordingsDir)) fs.mkdirSync(recordingsDir);
if (!fs.existsSync(responsesDir)) fs.mkdirSync(responsesDir);

// Function to transcribe audio using Deepgram
async function transcribeAudio(filePath) {
    try {
        const audioSource = {
            stream: fs.createReadStream(filePath),
            mimetype: 'audio/wav'
        };

        const { result, error } = await deepgram.listen.prerecorded.transcribeFile(
            audioSource,
              {
                model: "nova-2",
              }
            );

        return response.results.channels[0].alternatives[0].transcript;
    } catch (error) {
        console.error('Error transcribing with Deepgram:', error);
        throw error;
    }
}

// Function to generate speech using ElevenLabs API
async function generateSpeech(text, outputPath) {
    try {
        const response = await axios({
            method: 'POST',
            url: `https://api.elevenlabs.io/v1/text-to-speech/${process.env.VOICE_ID}/stream`,
            headers: {
                'Accept': 'audio/mpeg',
                'xi-api-key': process.env.ELEVEN_LABS_API_KEY,
                'Content-Type': 'application/json'
            },
            data: {
                text: text,
                model_id: 'eleven_monolingual_v1',
                voice_settings: {
                    stability: 0.5,
                    similarity_boost: 0.75
                }
            },
            responseType: 'arraybuffer'
        });

        fs.writeFileSync(outputPath, response.data);
        console.log('Speech generated successfully');
        return true;
    } catch (error) {
        console.error('Error generating speech:', error.response?.data || error.message);
        return false;
    }
}

app.post('/upload', upload.single('audio'), async (req, res) => {
    try {
        // Create timestamped filenames
        const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
        const wavPath = path.join(recordingsDir, `recording_${timestamp}.wav`);
        const responsePath = path.join(responsesDir, `response_${timestamp}.mp3`);
        
        // Create WAV file writer
        const writer = new wav.FileWriter(wavPath, {
            channels: 1,
            sampleRate: 16000,
            bitDepth: 16
        });
        
        writer.write(req.file.buffer);
        writer.end();
        
        console.log(`Saved recording to ${wavPath}`);
        
        // Transcribe with Deepgram
        const transcription = await transcribeAudio(wavPath);
        console.log('Transcription:', transcription);
        
        // Get ChatGPT response
        const completion = await openai.chat.completions.create({
            model: 'gpt-3.5-turbo',
            messages: [
                { role: 'system', content: 'You are ATLAS, a helpful AI assistant. Keep responses concise and natural.' },
                { role: 'user', content: transcription }
            ]
        });
        
        const textResponse = completion.choices[0].message.content;
        console.log('ChatGPT Response:', textResponse);
        
        // Generate speech from response
        await generateSpeech(textResponse, responsePath);
        
        res.json({
            status: 'success',
            transcription: transcription,
            response: textResponse,
            audioPath: responsePath
        });
        
    } catch (error) {
        console.error('Error:', error);
        res.status(500).json({ error: error.message });
    }
});

app.listen(port, () => {
    console.log(`Server running at http://localhost:${port}`);
    console.log(`Recordings will be saved to: ${recordingsDir}`);
    console.log(`Responses will be saved to: ${responsesDir}`);
});