#docker build -f DockerfileAWS  -t 445735284876.dkr.ecr.us-west-1.amazonaws.com/test-freeswitch-api   .
docker build -f ./Dockerfile.AWS.additional.modules -t 445735284876.dkr.ecr.us-west-1.amazonaws.com/test-frees  .

# LZ test
#docker build -f ~/src/finn-rails-api/Dockerfile  -t 445735284876.dkr.ecr.us-west-1.amazonaws.com/frees-finn  ../finn-rails-api/

#export AWS_ACCESS_KEY_ID='${{ secrets.AWS_ACCESS_KEY_ID }}'
#export AWS_SECRET_ACCESS_KEY='${{ secrets.AWS_SECRET_ACCESS_KEY }}'

#aws ecr get-login-password --region us-west-1 -p <default>\
#| docker login \
#    --username AWS \
#    --password-stdin 445735284876.dkr.ecr.us-west-1.amazonaws.com

aws ecr get-login-password --region us-west-1 | docker login --username AWS --password-stdin 445735284876.dkr.ecr.us-west-1.amazonaws.com

#docker push 445735284876.dkr.ecr.us-west-1.amazonaws.com/test-freeswitch-api
docker push 445735284876.dkr.ecr.us-west-1.amazonaws.com/test-frees
#docker push 445735284876.dkr.ecr.us-west-1.amazonaws.com/aida-docker
