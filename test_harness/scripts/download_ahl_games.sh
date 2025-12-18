#!/bin/bash
#
# Download David Ahl's BASIC Computer Games for testing CBASIC
#
# Source: https://github.com/coding-horror/basic-computer-games
# Original: "BASIC Computer Games" by David Ahl (Creative Computing, 1978)
#
# COPYRIGHT NOTICE:
# These programs are copyrighted by Creative Computing / David Ahl.
# They are downloaded here for PERSONAL TESTING PURPOSES ONLY.
# Do not redistribute these programs.
#
# The coding-horror/basic-computer-games repository contains both:
# - Original BASIC programs from the 1978 book
# - Modernized versions in various languages
#
# This script downloads only the original BASIC (.bas) files.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HARNESS_DIR="$(dirname "$SCRIPT_DIR")"
PROJECT_DIR="$(dirname "$HARNESS_DIR")"
DEST_DIR="$PROJECT_DIR/test_programs/ahl_games"
TEMP_DIR=$(mktemp -d)

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

echo "=============================================="
echo "Download David Ahl's BASIC Computer Games"
echo "=============================================="
echo ""
echo -e "${CYAN}Source:${NC} https://github.com/coding-horror/basic-computer-games"
echo -e "${CYAN}Book:${NC}   'BASIC Computer Games' (Creative Computing, 1978)"
echo -e "${CYAN}Dest:${NC}   $DEST_DIR"
echo ""
echo -e "${YELLOW}COPYRIGHT NOTICE:${NC}"
echo "These programs are copyrighted by Creative Computing / David Ahl."
echo "Downloaded for personal testing purposes only."
echo ""

# Check for git
if ! command -v git &> /dev/null; then
    echo -e "${RED}Error: git is required but not installed.${NC}"
    exit 1
fi

# Confirm with user
read -p "Continue with download? [y/N] " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Download cancelled."
    exit 0
fi

# Clone the repository (shallow clone for speed)
echo ""
echo "Cloning repository (this may take a moment)..."
git clone --depth 1 --quiet https://github.com/coding-horror/basic-computer-games.git "$TEMP_DIR/repo"

# Create destination directory
mkdir -p "$DEST_DIR"

# Track statistics
count=0
skipped=0

echo "Extracting BASIC files..."
echo ""

# Process each numbered game directory
for game_dir in "$TEMP_DIR/repo"/[0-9]*; do
    if [ -d "$game_dir" ]; then
        game_name=$(basename "$game_dir")

        # Look for .bas files in the game directory
        # Check common locations: root, csharp (sometimes has original), etc.
        bas_file=""

        # First try the root of the game directory
        for f in "$game_dir"/*.bas "$game_dir"/*.BAS; do
            if [ -f "$f" ]; then
                bas_file="$f"
                break
            fi
        done

        # If not found, check for a 'basic' or 'original' subdirectory
        if [ -z "$bas_file" ] || [ ! -f "$bas_file" ]; then
            for subdir in "basic" "original" "BASIC"; do
                for f in "$game_dir/$subdir"/*.bas "$game_dir/$subdir"/*.BAS; do
                    if [ -f "$f" ]; then
                        bas_file="$f"
                        break 2
                    fi
                done
            done
        fi

        if [ -n "$bas_file" ] && [ -f "$bas_file" ]; then
            # Create clean filename: lowercase, underscores
            dest_name=$(echo "$game_name" | tr '[:upper:]' '[:lower:]' | tr ' ' '_' | tr -cd '[:alnum:]_')
            dest_file="$DEST_DIR/${dest_name}.bas"

            cp "$bas_file" "$dest_file"
            count=$((count + 1))
            echo -e "  ${GREEN}+${NC} ${dest_name}.bas"
        else
            skipped=$((skipped + 1))
        fi
    fi
done

# Cleanup temporary directory
rm -rf "$TEMP_DIR"

echo ""
echo "=============================================="
echo "Download Complete"
echo "=============================================="
echo -e "  ${GREEN}Downloaded:${NC} $count programs"
if [ $skipped -gt 0 ]; then
    echo -e "  ${YELLOW}Skipped:${NC}    $skipped (no .bas file found)"
fi
echo -e "  ${CYAN}Location:${NC}   $DEST_DIR"
echo ""
echo "To test a game with CBASIC:"
echo "  $PROJECT_DIR/bin/cbasic $DEST_DIR/hamurabi.bas"
echo ""
echo "To compare with Microsoft BASIC (cbmbasic):"
echo "  cbmbasic $DEST_DIR/hamurabi.bas"
echo ""
echo "Popular games to try:"
echo "  - hamurabi.bas    (resource management)"
echo "  - superstartrek.bas (space combat)"
echo "  - lunar.bas       (lunar lander)"
echo "  - hangman.bas     (word guessing)"
echo "  - blackjack.bas   (card game)"
echo ""
