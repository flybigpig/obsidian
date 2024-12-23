---
promptId: 'modArtStation'
name: '🖼️ Generate a Trending on ArtStation photo'
description: 'This modifier will sample extra training data from the most-liked artwork from the website ArtStation. Images which trend on ArtStation are usually very visually-appealing as it means the ArtStation community enjoys those images, so filtering the data to produce images similar to those will greatly increase the quality of the generated art.'
author: 'Prompt Engineering Guide'
tags: 'photo, dalle-2, modifier'
version: 0.0.1
stream: false
disableProvider: true
---
```handlebars
{{#run "getPhoto" "r" "tg_selection"}}
 {{selection}}, Trending on ArtStation
{{/run}}
```
***
***
{{get "r"}}