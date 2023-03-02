docker build -t 445735284876.dkr.ecr.us-west-1.amazonaws.com/freeswitch-api .

export AWS_ACCESS_KEY_ID='${{ secrets.AWS_ACCESS_KEY_ID }}'
export AWS_SECRET_ACCESS_KEY='${{ secrets.AWS_SECRET_ACCESS_KEY }}'

aws ecr get-login-password --region us-west-1 \
| docker login \
    --username AWS \
    --password-stdin 445735284876.dkr.ecr.us-west-1.amazonaws.com

docker push 445735284876.dkr.ecr.us-west-1.amazonaws.com/freeswitch-api

