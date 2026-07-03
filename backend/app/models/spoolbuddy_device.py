from datetime import datetime

from sqlalchemy import Boolean, DateTime, Float, Integer, String, Text, func
from sqlalchemy.orm import Mapped, mapped_column

from backend.app.core.database import Base


class SpoolBuddyDevice(Base):
    """BamBuddy device registration for all hardware device types.

    Supports multiple device types:
    - "spoolbuddy": RPi/ESP32-based filament management stations (NFC + scale)
    - "printer-panel": Per-printer ESP32-S3 control panels (OLED + encoder)
    - "clear-plate": Legacy standalone clear-plate button boxes
    """

    __tablename__ = "spoolbuddy_devices"

    id: Mapped[int] = mapped_column(primary_key=True)
    device_id: Mapped[str] = mapped_column(String(50), unique=True, index=True)
    hostname: Mapped[str] = mapped_column(String(100))
    ip_address: Mapped[str] = mapped_column(String(45))
    firmware_version: Mapped[str | None] = mapped_column(String(20))
    has_nfc: Mapped[bool] = mapped_column(Boolean, default=True)
    has_scale: Mapped[bool] = mapped_column(Boolean, default=True)
    tare_offset: Mapped[int] = mapped_column(Integer, default=0)
    calibration_factor: Mapped[float] = mapped_column(Float, default=1.0)
    nfc_reader_type: Mapped[str | None] = mapped_column(String(20))
    nfc_connection: Mapped[str | None] = mapped_column(String(20))
    backend_url: Mapped[str | None] = mapped_column(String(255), nullable=True)
    display_brightness: Mapped[int] = mapped_column(Integer, default=100)
    display_blank_timeout: Mapped[int] = mapped_column(Integer, default=0)
    has_backlight: Mapped[bool] = mapped_column(Boolean, default=False)
    last_calibrated_at: Mapped[datetime | None] = mapped_column(DateTime)
    last_seen: Mapped[datetime | None] = mapped_column(DateTime)
    pending_command: Mapped[str | None] = mapped_column(String(50))
    pending_write_payload: Mapped[str | None] = mapped_column(Text, nullable=True)
    update_status: Mapped[str | None] = mapped_column(String(20), nullable=True)
    update_message: Mapped[str | None] = mapped_column(String(255), nullable=True)
    pending_system_payload: Mapped[str | None] = mapped_column(Text, nullable=True)
    nfc_ok: Mapped[bool] = mapped_column(Boolean, default=False)
    scale_ok: Mapped[bool] = mapped_column(Boolean, default=False)
    uptime_s: Mapped[int] = mapped_column(Integer, default=0)
    system_stats: Mapped[str | None] = mapped_column(Text, nullable=True)
    ssh_host_key: Mapped[str | None] = mapped_column(Text, nullable=True)

    # Fleet management columns (Issue #5)
    device_type: Mapped[str] = mapped_column(String(30), default="spoolbuddy")
    printer_id: Mapped[int | None] = mapped_column(Integer, nullable=True)
    device_config: Mapped[str | None] = mapped_column(Text, nullable=True)
    friendly_name: Mapped[str | None] = mapped_column(String(64), nullable=True)
    location: Mapped[str | None] = mapped_column(String(100), nullable=True)
    target_firmware: Mapped[str | None] = mapped_column(String(20), nullable=True)
    ota_status: Mapped[str] = mapped_column(String(20), default="current")

    created_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now())
    updated_at: Mapped[datetime] = mapped_column(DateTime, server_default=func.now(), onupdate=func.now())
