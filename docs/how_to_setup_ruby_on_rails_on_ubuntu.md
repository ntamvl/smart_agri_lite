# How to setup Ruby on Rails on Ubuntu

## Install Ruby

```bash
sudo apt-get update
sudo apt install build-essential rustc libssl-dev libyaml-dev zlib1g-dev libgmp-dev

# https://github.com/tj/n?tab=readme-ov-file#installing-nodejs-versions
curl -L https://bit.ly/n-install | bash
n lts

# https://github.com/rbenv/rbenv
git clone https://github.com/rbenv/rbenv.git ~/.rbenv
~/.rbenv/bin/rbenv init

rbenv install ruby 4.0.1

gem install rails -v 8.1.2
```
