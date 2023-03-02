FROM ruby:2.6.5

RUN apt-get update && apt-get install -y --no-install-recommends cmake lsof

ENV RAILS_ROOT /api

WORKDIR $RAILS_ROOT

COPY . $RAILS_ROOT

RUN bundle install --jobs 20 --retry 5

RUN rm -f tmp/pids/server.pid

CMD ["rails", "s", "-b", "0.0.0.0"]
