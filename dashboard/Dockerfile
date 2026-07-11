FROM python:3.10-slim

WORKDIR /app

RUN pip install --no-cache-dir flask

COPY . /app/dashboard

EXPOSE 5000

CMD ["python", "/app/dashboard/app.py"]
