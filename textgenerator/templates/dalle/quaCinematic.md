---
promptId: 'quaCinematic'
name: '🖼️ Generate a cinematic movie photo'
description: 'Adds a very atmospheric movie-like feel to the image, with great color tones and image composure, and can also add nice background blur and pretty camera angles.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2,quality'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Cinematic Movie Photograph
{{/run}}
```
***
***
{{get "r"}}