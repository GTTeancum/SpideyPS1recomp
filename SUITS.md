# Suits (v1.1)

Ticked: the 1.1 suit is approved as released (checked in a clean 1.1 install of Spider-Man 1).
Unticked: needs attention; assess these one at a time.

## Reskins (not Spider-Man Unlimited)

- [x] 1967 TV Series (`1967-tv-series`)
- [x] Amazing (TASM) (`amazing-tasm`)
- [x] America's Spider (`americas-spider`)
- [x] Ben Reilly (Street) (`ben-reilly-street`)
- [x] John Romita Sr. (`friendly-neighbor`)
- [x] Kaine (`kaine`)
- [x] MCU Far From Home (`mcu-far-from-home`)
- [x] Quick Change (red) (`quick-change-red`)
- [x] Raimi (red & blue) (`raimi-red-blue`) 
- [x] Raimi (symbiote) (`raimi-symbiote`)
- [x] Steve Ditko (`steve-ditko`)
- [x] Superior (`superior`)
- [x] The Human Spider (`the-human-spider`)

## Spider-Man Unlimited

- [x] 1602 (`smu-1602`)
- [x] 2099 NEW (`smu-2099-new`)
- [x] 2211 (`smu-2211`)
- [x] AGENT NINE (`smu-agent-nine`)
- [x] AIKMAN (`smu-aikman`)
- [x] AMAZING SPIDER (`smu-amazing-spider`)
- [x] ARACHNIDO (`smu-arachnido`)
- [x] ARACHNOMAN (`smu-arachnoman`)
- [x] BEN UNMASKED (`smu-ben-reilly-unmasked`) 
- [x] BIG TIME (`smu-bigtime`)
- [x] BIG TIME RED (`smu-bigtime-red`)
- [x] BULLETPOINTS (`smu-bulletpoints`)
- [x] BUZZ (`smu-buzz`)
- [x] CAPTAIN (`smu-captain`)
- [x] CIVILWARPETERPARKER (`smu-civilwarpeterparker`)
- [x] CYBORG (`smu-cyborg`)
- [x] EARTH62412 (`smu-earth62412`) 
- [x] EARTHX (`smu-earthx`)
- [x] ENDOFEARTH (`smu-endofearth`)
- [x] EZEKIEL (`smu-ezekiel`) 
- [x] FEARITSELF (`smu-fearitself`) 
- [x] FUTUREFOUNDATION (`smu-futurefoundation`)
- [x] GOD (`smu-god`) 
- [x] HOBARTBROWN (`smu-hobartbrown`)
- [x] HOBIE (`smu-hobie`)
- [x] HOMEMADE (`smu-homemade`)
- [x] HORNET (`smu-hornet`)
- [x] HOUSEOFM (`smu-houseofm`)
- [x] INDIA (`smu-india`)
- [x] IRONSPIDER (`smu-ironspider`)
- [x] IRONSPIDERNEWW #094 (`smu-ironspidernewwithtentackles`)
- [x] KUNG FU (`smu-kung-fu`)
- [x] MANGAVERSE (`smu-mangaverse`)
- [x] MASKEDMARVEL (`smu-maskedmarvel`)
- [x] MECH (`smu-mech`)
- [x] MILESMORALES (`smu-milesmorales`)
- [x] MODERNSCARLET (`smu-modernscarlet`)
- [x] NATIVE AMERICAN (`smu-native-american`)
- [x] NEW YEAR SPIDERMAN (`smu-new-year-spiderman`)
- [x] NEWARMOR (`smu-newarmor`)
- [x] NOIR (`smu-noir`)
- [x] NOIRNEW (`smu-noirnew`)
- [x] PETER PORKER U #145 (`smu-peter-porker-unmasked`)
- [x] PETER URICH (`smu-peter-urich`)
- [x] PUNK (`smu-punk`)
- [x] SCARLET SPIDER (`smu-scarlet-spider`)
- [x] SCARLET HAM (`smu-scarlet-spiderham`)
- [x] SCARLET SPIDER #167 (`smu-scarlet-spiderham-benreilly`)
- [x] SECRETWAR (`smu-secretwar`)
- [x] SIXARMED SPIDERMAN (`smu-sixarmed-spiderman`)
- [x] SPACESUIT (`smu-spacesuit`) 
- [x] SPIDER COWBOY (`smu-spider-cowboy`)
- [x] SPIDER HERC (`smu-spider-herc`) 
- [x] SPIDER NINJA (`smu-spider-ninja`)
- [x] SPIDER SPINER (`smu-spider-spiner`)
- [x] SPIDERARMOR (`smu-spiderarmor`)
- [x] Spider-Ham (`smu-spiderham`)
- [x] SPIDERHAM 2099 (`smu-spiderham-2099`)
- [x] SPIDERJAMESON (`smu-spiderjameson`)
- [x] SPIDERKNIGHT (`smu-spiderknight`)
- [x] SPIDERMAN 2020 (`smu-spiderman-2020`)
- [x] SPIDERMAN NEWDESIGN (`smu-spiderman-newdesign`)
- [x] SPIDERMAN SURV #197 (`smu-spiderman-surveillancesuit`) 
- [x] SPIDERMANHOMECOMING (`smu-spidermanhomecoming`) --long feet
- [x] STEEL (`smu-steel`)
- [x] THE SPIDER PET #215 (`smu-the-spider-peter-parker`)
- [x] TIMESLEEP (`smu-timesleep`) 
- [x] UK (`smu-uk`)
- [x] ULTIMATE SPIDERHAM (`smu-ultimate-spiderham`)
- [x] UNCLEB (`smu-uncleb`)

## November 22 SMU pose and foot repairs

The original PS1 boots reach 407 model units forward from the native ankle in the neutral pose. The first foot repair covered 20 suits in commit `8b729f8`. Earth-62412 and Earth X were outside that batch. Their left and right boots reached 663/679 and 570/570 units respectively.

Both ports use the shared converted actors. The conversion now measures each human boot after skinning and shortens excess reach along the floor through its original ankle and toe weights. This keeps the sole height and leaves the previously reviewed boots within their original two-unit rounding tolerance. Forty-four suits received foot corrections. All six Spider-Ham and Peter Porker actors retain their original bytes.

The previous calibration aimed the source upper spine at the ribcage using the native chest-to-head direction. It passed that extra pitch into the neck and head. The first posture repair compared chest to head in both skeletons and gave the head its own native rotation. The bank side view subsequently showed that the torso still hunched above the belt. All 64 human rigs retain their original bone lengths and skin weights.

The staged SM1 and SM2 model files matched the reviewed source. Earth-62412 and Earth X passed all 4,196 frames of the original SM1 animation bank and were checked in hidden, muted runs of both games. The bank comparison uses one decoded bank-idle frame for all three actors. The gameplay images are separate native captures. [Bank pose comparison](.local/suit-proportions/bank-pose-comparison.png) · [Game captures](.local/suit-proportions/game-captures.png) · [Roster measurements](.local/suit-proportions/validation.json)


### Bank torso follow-up

The SMU waist (`Spine1`) followed the pelvis and the lower chest (`Spine2`) followed the abdomen. This concentrated the lean above the waist. The human suit conversion now drives the waist from the native abdomen and both chest joints from the native chest. It aligns the authored pelvis-to-head direction with the stock neutral trunk as a whole. The head continues to use the native head rotation. The same calibration runs during conversion and when repairing existing actors.

The regression test measures the rear edge of the skinned torso from belt to shoulders in four poses recorded at the SM1 bank. In the idle pose, the median RMS difference in the rear outline across the 64 human suits fell from 33% to 5% of the measured torso height. Suits with a backpack or extra arms have a different outer silhouette by design. Earth-62412 and Earth X were also checked in the SM1 bank and on the first SM2 rooftop in hidden, muted runs. For the in-game side comparison, the test harness held the bank position and idle animation. All 18 native joint matrices matched byte for byte across the default, previous and revised actors. The two staged catalogs match the shared source. [In-game bank side views](.local/suit-back/native-bank-profile.png) · [Decoded bank side views](.local/suit-back/bank-comparison.png) · [Native game captures](.local/suit-back/native-gameplay.png) · [Regression results](.local/suit-back/validation.json)

## November 23 skull seam repairs

Civil War Parker, Earth-X, Earth-62412, Future Foundation, Modern Scarlet, Spider-Spinner, Spider-Ham - Ben R., Spider-Man 2020, Spidey Homecoming, Spidey New Design, Surveillance Suit, and UK had visible seams at the crown or down the back of the head. The mesh vertices already met. The face texture coordinates along that edge sampled mismatched colors or the empty margin around the mask texture.

The conversion now pairs those coordinates on each mask. Earth-X keeps its modeled rear fastening, and Spider-Ham - Ben R. keeps its original ears, rig, and proportions. All twelve actors were selected in both costume viewers. Follow-up SM1 testing still showed problems on Earth-62412, Spidey New Design, and UK. [Before and after close-ups](.local/skull/skull-before-after.jpg) · [SM1 costume viewer](.local/skull/sm1-viewer.jpg) · [SM2 costume viewer](.local/skull/sm2-viewer.jpg)

### SM1 forehead and crown follow-up

The UK mask overlay placed the facial center at pixel 776 on the 1024-pixel texture while the painted center lay near 782. Spidey New Design had the same offset. Both masks now place the painted and geometric centers at 784. Their vertical web above the eyes joins the next horizontal strand. Earth-62412 uses its own mask layout centered at 624. The SM1 costume viewer was checked with all three staged assets.

The conversion pairs corresponding left and right mask vertices, preserves the separate eye artwork, and matches the colors sampled across the crown. The embedded 128-pixel textures use the corrected artwork as well. The SM2 staging update is pending the current playtest. [UK UV overlay](.local/skull/follow/uk-uv-overlay.png) · [Face and crown comparisons](.local/skull/follow/face-and-crown.jpg) · [SM1 costume viewer](.local/skull/follow/native-front.jpg)
