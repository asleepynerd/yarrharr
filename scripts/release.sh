#!/bin/bash

# Check if version argument is provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.2.3"
    exit 1
fi

VERSION=$1

# Validate version format
if ! [[ $VERSION =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Error: Version must be in format X.Y.Z (e.g., 1.2.3)"
    exit 1
fi

# Update version.txt
echo $VERSION > version.txt

# Prompt for changelog entry
echo "Enter changelog entry (press Ctrl+D when done):"
CHANGELOG_ENTRY=$(cat)

# Update CHANGELOG.md
DATE=$(date +%Y-%m-%d)
sed -i "" "s/## \[Unreleased\]/## [Unreleased]\n\n## [$VERSION] - $DATE\n$CHANGELOG_ENTRY/" CHANGELOG.md

# Commit changes
git add version.txt CHANGELOG.md
git commit -m "Bump version to $VERSION"

# Create and push tag
git tag -a "v$VERSION" -m "Release version $VERSION"
git push origin main "v$VERSION"

echo "Release v$VERSION created and pushed!" 