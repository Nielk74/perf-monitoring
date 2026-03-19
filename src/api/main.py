"""
src/api/main.py — FastAPI application entry point.

Run:
    uvicorn src.api.main:app --reload --port 8000
"""

from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from src.api.routes import ingestion, analytics, metadata

app = FastAPI(
    title="STAR Perf Monitoring API",
    version="1.0.0",
    description="Performance analytics for the STAR trading system.",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(ingestion.router, prefix="/api/ingestion")
app.include_router(analytics.router, prefix="/api/analytics")
app.include_router(metadata.router,  prefix="/api/metadata")


@app.exception_handler(Exception)
async def generic_exception_handler(request: Request, exc: Exception):
    return JSONResponse(
        status_code=500,
        content={
            "status": 500,
            "title": "Internal Server Error",
            "detail": str(exc),
        },
    )


@app.get("/health")
def health():
    return {"status": "ok"}
